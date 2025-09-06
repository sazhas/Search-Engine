#include "csolver.h"

#include <cassert>
#include <chrono>
#include <iomanip>   // for std::setprecision and std::fixed
#include <iostream>
#include <vector>

#include <pthread.h>
#include <unistd.h>

#include <arpa/inet.h>

#include "../lib/HashTable.h"

// Constructor sets up server socket and binding.
CSolver::CSolver(const std::string& ip, unsigned int port, const std::vector<IndexBlob*>& blobs)
    : fd_serv { -1 }
    , blobs { blobs } {
    addr_serv.sin_family = AF_INET;
    addr_serv.sin_port = htons(port);
    addr_serv.sin_addr.s_addr = ip.empty() ? INADDR_ANY : inet_addr(ip.c_str());

    fd_serv = socket(AF_INET, SOCK_STREAM, 0);
    if (fd_serv < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    int opt = 1;
    if (setsockopt(fd_serv, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt");
        close(fd_serv);
        exit(EXIT_FAILURE);
    }

    const int enable = 1;
    if (setsockopt(fd_serv, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    if (::bind(fd_serv, reinterpret_cast<struct sockaddr*>(&addr_serv), sizeof(addr_serv)) < 0) {
        perror("bind");
        close(fd_serv);
        exit(EXIT_FAILURE);
    }
}

CSolver::~CSolver() {
    if (fd_serv >= 0) {
        close(fd_serv);
    }
}

void CSolver::serialize_results(int fd_client, const std::vector<RankingResult>& results) {
    std::cout << "SENDING RESULTS\n";

    uint32_t count_net = htonl(static_cast<uint32_t>(results.size()));
    send_looping_crashing(fd_client, reinterpret_cast<const char*>(&count_net), sizeof(count_net));

    const char NL = '\n';

    for (const auto& result : results) {
        /* URL\n */
        send_looping_crashing(fd_client, result.url, strlen(result.url));
        send_looping_crashing(fd_client, &NL, 1);

        /* TITLE\n */
        send_looping_crashing(fd_client, result.title, strlen(result.title));
        send_looping_crashing(fd_client, &NL, 1);

        /* SCORE (binary double, network byte‑order) */
        static_assert(sizeof(double) == 8, "unexpected double size");
        cout << result.score << endl;
        uint64_t bits;
        std::memcpy(&bits, &result.score, sizeof(bits));
        bits = my_htonll(bits);   // big‑endian
        send_looping_crashing(fd_client, reinterpret_cast<const char*>(&bits), sizeof(bits));
    }
}

void CSolver::init_instance(const std::string& ip, unsigned int port, const std::vector<IndexBlob*>& blobs) {
    assert(!instance);
    instance = new CSolver(ip, port, blobs);
}

CSolver* CSolver::get_instance() {
    assert(instance);
    return instance;
}

#ifndef TEST_NETWORK_ONLY
std::vector<RankingResult> mergeSortedArrays(const std::vector<std::vector<RankingResult>>& arrays) {
    int k = arrays.size();
    std::vector<int> indices(k, 0);   // track current position in each array
    std::vector<RankingResult> result;

    while (result.size() < MAX_RESULTS) {
        double maxVal = std::numeric_limits<double>::lowest();
        int minArray = -1;

        // Find the smallest element among current heads
        for (int i = 0; i < k; ++i) {
            if (indices[i] < arrays[i].size()) {
                if (arrays[i][indices[i]].score > maxVal) {
                    maxVal = arrays[i][indices[i]].score;
                    minArray = i;
                }
            }
        }

        if (minArray == -1) break;   // all arrays are exhausted

        result.push_back(arrays[minArray][indices[minArray]]);
        indices[minArray]++;   // move forward in the chosen array
    }

    return result;
}
#endif /* TEST_NETWORK_ONLY */

bool CSolver::process_client_request(int fd_client) {
    try {
        auto start = std::chrono::high_resolution_clock::now();

        Expr_AST ast(fd_client);

#ifndef TEST_NETWORK_ONLY
        std::vector<std::vector<RankingResult>> all;
#endif

        u_int32_t results_size = 0;

        for (IndexBlob* b : blobs) {
            ISR_Tree tree(b, &ast);

#ifndef TEST_NETWORK_ONLY
            Ranker::Ranker rk(b);
            auto partial = rk.RankResults(&tree);
            results_size += partial.size();

            all.push_back(partial);

            if (results_size > MAX_RANKED_DOCS) {
                break;
            }
#endif /* TEST_NETWORK_ONLY */
        }

#ifndef TEST_NETWORK_ONLY
        serialize_results(fd_client, mergeSortedArrays(all));
#endif

        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> elapsed = end - start;
        std::cout << "[Timing] process_client_request took " << std::fixed << std::setprecision(2) << elapsed.count()
                  << " seconds\n";

        return true;
    } catch (...) {
        std::cerr << "Error processing client request\n";
        return false;
    }
}


void CSolver::serve_requests() {
    if (listen(fd_serv, SOMAXCONN) < 0) {
        perror("listen");
        close(fd_serv);
        std::exit(EXIT_FAILURE);
    }
    std::cout << "Server listening …\n";

    while (true) {
        sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);
        int fd = accept(fd_serv, reinterpret_cast<sockaddr*>(&client_addr), &addr_len);
        if (fd < 0) {
            perror("accept");
            continue;
        }

        char ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, ip, sizeof(ip));
        std::cout << "Connection from " << ip << ':' << ntohs(client_addr.sin_port) << '\n';

        if (!process_client_request(fd)) std::cerr << "Failed to handle query from " << ip << '\n';

        close(fd);   // done with the client
    }
}
