#ifndef PARSER_H
#define PARSER_H

#include "HtmlParser.h"
#include <cstddef>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string>
#include <pthread.h>

#include "../lib/BloomFilter.h"
#include "../lib/mutex.h"
#include "../lib/cv.h"
#include "../indexer/Indexer.hpp"


class Parser;

struct ParseArgs {
    std::string url;
    std::string html;
    int depth;
};

struct IndexSave {
    int chunk_count;
    Index* index;
    Parser* parser;
    time_t time;
};

// The Parser class continuously listens for HTML data from a crawler.
// For each connection, it spawns a thread (talk) to read and validate the HTTP
// header, then it reads the HTML body and spawns another thread that creates an
// instance of the HTML parser class.
class Parser {
public:

    // Constructor: Initializes the listening socket on the given port.
    Parser();
    ~Parser();

    void save();
    void resetParserThreadsIfNeeded();

    int index_chunk_count;
    std::vector<ParseArgs*> toParse;
    std::vector<HtmlParser*> parsedPages;
    std::vector<IndexSave*> toSave;
    size_t total_parsed = 0;
    size_t total_indexed = 0;
    size_t total_saved = 0;

private:

    int listenSocket;
    struct sockaddr_in listenAddress;
    int port; // listening port
    std::vector<pthread_t> save_threads;

    Bloomfilter filter;
    Mutex filter_lock;

    Mutex talkLock;
    CV talkCV;
    std::vector<int> talkingSockets;

    Mutex parsedPagesLock;
    CV parsedPagesCV;

    Mutex toParseLock;
    CV toParseCV;

    Mutex toSaveLock;
    CV toSaveCV;

    struct SendUrl {
        std::string url;
        int depth;
    };

    struct Crawler {
        std::string ip;
        std::vector<SendUrl*> links;
        Mutex lock;
        CV cv;
    };
    std::vector<Crawler> crawlers;

    struct SendArgs {
        Parser* parser;
        size_t crawler_index;
    };

    // Structures for passing thread arguments.
    struct TalkArgs {
        int socket;
        Parser* parser;
    };

    // Thread functions.
    static void* listener(void* arg);
    static void* talk(void* _args);
    static void* parserThread(void* arg);
    static void* IndexSaveThread(void* arg);
    static void* SendLinkThread(void* arg);
    static void* async_index_save(void* arg);

    void sendLinksList(const HtmlParser& parser, int depth, int port);
    void readPeers();

    // Disallow copying.
    Parser(const Parser&) = delete;
    Parser& operator=(const Parser&) = delete;
};

#endif // PARSER_H
