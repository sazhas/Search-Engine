#include "Parser.h"

#include <cctype>   // for isspace
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <fcntl.h>
#include <pthread.h>
#include <sched.h>
#include <string>
#include <sys/mman.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

#include "../lib/file.h"
#include "../lib/constants.h"
#include "../lib/iostream.h"
#include "HtmlParser.h"

// Constructor: Set up the listening socket.
Parser::Parser()
    : port(PARSER_PORT)
    , index_chunk_count(0)
    , filter(BLOOM_FRONTIER_SIZE, FRONTIER_FP_RATE)
    , filter_lock()
    , parsedPagesLock()
    , parsedPages()
    , crawlers() {

    if (!access(PARSER_FILTER_FILE, F_OK)) {
        filter = Bloomfilter(PARSER_FILTER_FILE);
    }

    if (!access(PARSER_PEERS_FILE, F_OK)) {
        readPeers();
    } else {
        crawlers.resize(1);
        crawlers[0].ip = "127.0.0.1";
    }

    // Get starting index number
    while(true) {
        std::string filename(INDEX_CHUNK_NAME);
        filename += std::to_string(index_chunk_count);
        filename += ".bin";

        if (access(filename.c_str(), F_OK)) {
            break;
        }
        index_chunk_count++;
    }

    listenSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (listenSocket < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    const int enable = 1;
    if (setsockopt(listenSocket, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    std::memset(&listenAddress, 0, sizeof(listenAddress));
    listenAddress.sin_family = AF_INET;
    listenAddress.sin_port = htons(port);
    listenAddress.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(listenSocket, (struct sockaddr*) &listenAddress, sizeof(listenAddress)) < 0) {
        perror("bind");
        exit(EXIT_FAILURE);
    }
    if (listen(listenSocket, NUM_PARSER_TALK_THREADS) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    pthread_t listenerTid;
    if (pthread_create(&listenerTid, nullptr, Parser::listener, this) != 0) {
        perror("pthread_create");
        exit(EXIT_FAILURE);
    }
    pthread_detach(listenerTid);

    for (int i = 0; i < NUM_PARSER_TALK_THREADS; ++i) {
        pthread_t talking_thread;
        if (pthread_create(&talking_thread, nullptr, Parser::talk, this) != 0) {
            perror("pthread_create");
            exit(EXIT_FAILURE);
        }
        pthread_detach(talking_thread);
    }

    for (int i = 0; i < NUM_PARSE_THREADS; ++i) {
        pthread_t parser_thread;
        if (pthread_create(&parser_thread, nullptr, Parser::parserThread, this) != 0) {
            perror("pthread_create");
            exit(EXIT_FAILURE);
        }
        save_threads.push_back(parser_thread);
    }

    // Even number of threads per crawler
    int num_send_threads = (NUM_SEND_THREADS / crawlers.size()) * crawlers.size();
    for (int i = 0; i < num_send_threads; ++i) {
        auto args = new SendArgs{this, i % crawlers.size()};
        pthread_t send_thread;
        if (pthread_create(&send_thread, nullptr, Parser::SendLinkThread, args) != 0) {
            perror("pthread_create");
            exit(EXIT_FAILURE);
        }
        pthread_detach(send_thread);
    }

    for (int i = 0; i < NUM_INDEX_SAVE_THREADS; ++i) {
        pthread_t index_write;
        if (pthread_create(&index_write, nullptr, Parser::async_index_save, this) != 0) {
            perror("pthread_create");
            exit(EXIT_FAILURE);
        }
        pthread_detach(index_write);
    }

    for (int i = 0; i < NUM_INDEX_SAVE_THREADS; ++i) {
        pthread_t index_save;
        if (pthread_create(&index_save, nullptr, Parser::IndexSaveThread, this) != 0) {
            perror("pthread_create");
            exit(EXIT_FAILURE);
        }
        pthread_detach(index_save);
    }
}

Parser::~Parser() {
    close(listenSocket);
}

// The listener thread: continuously accepts new connections.
void* Parser::listener(void* arg) {
    Parser* self = static_cast<Parser*>(arg);
    struct sockaddr_in clientAddr;
    socklen_t addrLen = sizeof(clientAddr);

    while (true) {
        int clientSocket = accept(self->listenSocket, (struct sockaddr*) &clientAddr, &addrLen);
        if (clientSocket < 0) {
            continue;   // Accept failed; try again.
        }

        self->talkLock.lock();
        self->talkingSockets.push_back(clientSocket);
        self->talkCV.signal();
        self->talkLock.unlock();
    }
    return nullptr;
}

void* Parser::async_index_save(void* arg) {
    auto parser = static_cast<Parser*>(arg);

    while(true) {
        parser->toSaveLock.lock();
        while(parser->toSave.empty()) {
            parser->toSaveCV.wait(parser->toSaveLock);
        }
        auto index_save = parser->toSave.back();
        parser->toSave.pop_back();
        parser->toSaveLock.unlock();

        auto index = index_save->index;

        std::string name = INDEX_CHUNK_NAME;
        name += std::to_string(index_save->chunk_count);
        name += ".bin";

        time_t mid = time(nullptr);

        auto doc_count = index->DocumentsInIndex;
        irs::cout << name << " indexed " << doc_count << " documents to save after " << mid - index_save->time << irs::endl;

        parser->save();
        IndexFile file(name.c_str(), index);
        delete index;
        file.close_file();

        time_t end = time(nullptr);

        irs::cout << doc_count << " pages written to " << name << " after " << end - mid << " seconds" <<  irs::endl;
        delete index_save;

        // Update stats
        parser->toSaveLock.lock();
        parser->total_saved += doc_count;
        parser->toSaveLock.unlock();
    }

    return nullptr;
}

void* Parser::IndexSaveThread(void* arg) {
    auto parser = static_cast<Parser*>(arg);

    // Index forever
    while(true) {
        time_t begin = time(nullptr);

        auto index = new Index{};

        auto index_args = new IndexSave{};
        index_args->index = index;
        index_args->time = begin;
        index_args->parser = parser;

        // While we have not met threshold
        while (index->DocumentsInIndex < MIN_PAGES_PER_CHUNK) {
            parser->parsedPagesLock.lock();
            while (parser->parsedPages.empty()) {
                parser->parsedPagesCV.wait(parser->parsedPagesLock);
            }
            auto html = parser->parsedPages.back();
            parser->parsedPages.pop_back();
            parser->parsedPagesLock.unlock();

            index->Insert(html);
            delete html;

            // Update stats
            parser->parsedPagesLock.lock();
            parser->total_indexed++;
            parser->parsedPagesLock.unlock();
        }

        parser->parsedPagesLock.lock();
        index_args->chunk_count = parser->index_chunk_count++;
        parser->parsedPagesLock.unlock();

        parser->toSaveLock.lock();
        parser->toSave.push_back(index_args);
        parser->toSaveCV.signal();
        parser->toSaveLock.unlock();
    }
    return nullptr;
}

void Parser::save() {
    filter_lock.lock();
    filter.save(PARSER_FILTER_FILE);
    filter_lock.unlock();
}

// The talk thread: reads the HTTP header and body, then spawns the parser thread.
void* Parser::talk(void* _args) {
    auto parser = static_cast<Parser*>(_args);
    auto& filter = parser->filter;
    auto& filter_lock = parser->filter_lock;
    auto& talkLock = parser->talkLock;
    auto& talkCV = parser->talkCV;
    auto& talkingSockets = parser->talkingSockets;

    while (true) {
        talkLock.lock();
        while (talkingSockets.empty()) {
            talkCV.wait(talkLock);
        }
        int sock = talkingSockets.back();
        talkingSockets.pop_back();
        talkLock.unlock();

        std::string url;
        uint32_t url_size;
        if (recv(sock, &url_size, sizeof(url_size), MSG_WAITALL) <= 0) {
            close(sock);
            continue;
        }
        url_size = ntohl(url_size);

        uint32_t url_depth;
        if (recv(sock, &url_depth, sizeof(url_depth), MSG_WAITALL) <= 0) {
            close(sock);
            continue;
        }
        url_depth = ntohl(url_depth);

        url.resize(url_size);
        if (recv(sock, url.data(), url_size, MSG_WAITALL) <= 0) {
            close(sock);
            continue;
        }

        filter_lock.lock();
        if (filter.contains(url)) {
            filter_lock.unlock();
            close(sock);
            continue;
        }
        filter.insert(url);
        filter_lock.unlock();

        std::string body;
        uint32_t body_size;
        if (recv(sock, &body_size, sizeof(body_size), MSG_WAITALL) <= 0) {
            close(sock);
            continue;
        }
        body_size = ntohl(body_size);
        body.resize(body_size);

        if (recv(sock, body.data(), body_size, MSG_WAITALL) <= 0) {
            close(sock);
            continue;
        }

        // Close the connection; no need to send a response.
        close(sock);

        // Spawn a new thread to process the HTML with your parser.
        ParseArgs* pargs = new ParseArgs;
        pargs->url = std::move(url);
        pargs->html = std::move(body);
        pargs->depth = url_depth;

        parser->toParseLock.lock();
        parser->toParse.push_back(pargs);
        parser->toParseCV.signal();
        parser->toParseLock.unlock();
    }

    return nullptr;
}

template<typename T>
void cleanup(void* p) {
    delete static_cast<T*>(p);
};
//
// The parser thread: creates an instance of HtmlParser.
void* Parser::parserThread(void* arg) {
    auto parser = static_cast<Parser*>(arg);
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, nullptr);
    pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, nullptr);

    while (true) {
        parser->toParseLock.lock();
        while (parser->toParse.empty()) {
            parser->toParseCV.wait(parser->toParseLock);
        }

        auto pargs = parser->toParse.back();
        pthread_cleanup_push(cleanup<ParseArgs>, pargs);
        parser->toParse.pop_back();
        parser->toParseLock.unlock();

        HtmlParser* html_parser = new HtmlParser(pargs->html.data(), pargs->html.size());
        pthread_cleanup_push(cleanup<HtmlParser>, html_parser);
        html_parser->pageURL = pargs->url;

        parser->sendLinksList(*html_parser, pargs->depth + 1, FRONTIER_PORT);

        parser->parsedPagesLock.lock();
        parser->parsedPages.push_back(html_parser);
        pthread_cleanup_pop(0);
        html_parser = nullptr;
        parser->total_parsed++;
        parser->parsedPagesCV.signal();
        parser->parsedPagesLock.unlock();

        delete pargs;
        pthread_cleanup_pop(0);
        pargs = nullptr;
    }
    return nullptr;
}

void* Parser::SendLinkThread(void* arg) {
    auto args = static_cast<SendArgs*>(arg);
    auto parser = args->parser;
    auto index = args->crawler_index;
    delete args;

    auto& crawler = parser->crawlers[index];

    // Set up the destination address (localhost).
    sockaddr_in destAddr;
    std::memset(&destAddr, 0, sizeof(destAddr));
    destAddr.sin_family = AF_INET;
    destAddr.sin_port = htons(FRONTIER_PORT);

    int sleep_time = 1;
    // Continuously try to connect to crawler
    while (true) {
        // Create socket.
        int sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock < 0) {
            perror("socket");

            sleep(sleep_time);
            if (sleep_time < 30) sleep_time *= 2;
            continue;
        }

        if (inet_pton(AF_INET, crawler.ip.c_str(), &destAddr.sin_addr) <= 0) {
            irs::cerr << crawler.ip << " not supported" << irs::endl;
            close(sock);
            return nullptr;
        }

        // Connect to the server.
        if (connect(sock, (struct sockaddr*)&destAddr, sizeof(destAddr)) < 0) {
            perror("connect");
            close(sock);

            sleep(sleep_time);
            if (sleep_time < 30) sleep_time *= 2;
            continue;
        }

        sleep_time = 1;

        // While connected, send links
        while(true) {
            crawler.lock.lock();
            while (crawler.links.empty()) {
                crawler.cv.wait(crawler.lock);
            }
            auto data = crawler.links.back();
            crawler.links.pop_back();
            crawler.lock.unlock();

            auto url = data->url;
            auto depth = data->depth;
            delete data;

            // Prepare the payload length.
            uint32_t len = htonl(static_cast<uint32_t>(url.size()));
            if (send(sock, &len, sizeof(len), MSG_NOSIGNAL) <= 0) {
                perror("send length");
                break;
            }

            // Prepare the payload length.
            uint32_t url_depth = htonl(static_cast<uint32_t>(depth));
            if (send(sock, &url_depth, sizeof(url_depth), MSG_NOSIGNAL) <= 0) {
                perror("send depth");
                break;
            }

            // Send the payload.
            ssize_t sent = send(sock, url.c_str(), url.size(), MSG_NOSIGNAL);
            if (sent <= 0) {
                perror("send payload");
                break;
            }
        }

        // Sending failed, close socket
        close(sock);
    }

    return nullptr;
};

// Function to send a list of links in one message.
void Parser::sendLinksList(const HtmlParser& parser, int depth, int port) {
    for (const auto& link: parser.links) {
        std::string url;
        if (link.URL.compare(0, 4, "http") != 0) {
            if (parser.base.empty()) {
                // Not an http url so skip it
                continue;
            } else {
                url = parser.base + link.URL;
            }
        } else {
            url = link.URL;
        }

        // Randomly assign url
        auto crawler_index = rand() % crawlers.size();
        auto& crawler = crawlers[crawler_index];
        crawler.lock.lock();
        crawler.links.push_back(new SendUrl{url, depth});
        crawler.cv.signal();
        crawler.lock.unlock();
    }
}

void Parser::readPeers() {
    int fd = open(PARSER_PEERS_FILE, O_RDONLY);
    if (fd < 0) {
        perror("open");
        exit(EXIT_FAILURE);
    }

    auto size = file_size(fd);
    if (!size) {
        close(fd);
        crawlers.resize(1);
        crawlers[0].ip = "127.0.0.1";
        return;
    }

    char* map = (char*) mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
    if (map == MAP_FAILED)
        exit(2);

    std::vector<char*> ips;
    char* begin = map;
    for (char* end = map; end < map + size; ++end) {
        if (*end == '\n') {
            *end = '\0';
            ips.push_back(begin);
            begin = end + 1;
        }
    }

    crawlers.resize(ips.size());
    for (int i = 0; i < ips.size(); ++i) {
        crawlers[i].ip = ips[i];
    }

    munmap(map, size);
}

void Parser::resetParserThreadsIfNeeded() {
    toParseLock.lock();
    if (toParse.size() > MIN_PAGES_PER_CHUNK) {
        for (int i = 0; i < NUM_PARSE_THREADS; ++i) {
            pthread_cancel(save_threads[i]);
            pthread_join(save_threads[i], nullptr);
            pthread_t parser_thread;
            if (pthread_create(&parser_thread, nullptr, Parser::parserThread, this) != 0) {
                perror("pthread_create");
                exit(EXIT_FAILURE);
            }
            save_threads[i] = parser_thread;
        }
        irs::cout << "Parser threads were frozen. Restarting.\n" << irs::endl;
    }
    toParseLock.unlock();
}

int main() {
    time_t begin = time(nullptr);
    Parser parser {};
    while (true) {
        sleep(PARSER_SAVE_TIME);
        irs::cout << "\nParser is alive for " << time(nullptr) - begin << " seconds";
        irs::cout << "\nParser toParse size: " << parser.toParse.size();
        irs::cout << "\nParser parsedPages size: " << parser.parsedPages.size();
        irs::cout << "\nParser toSave size: " << parser.toSave.size();
        irs::cout << "\nTotal parsed: " << parser.total_parsed;
        irs::cout << "\nTotal indexed: " << parser.total_indexed;
        irs::cout << "\nTotal saved: " << parser.total_saved << irs::endl;

        parser.resetParserThreadsIfNeeded();
    }

    return 0;
}
