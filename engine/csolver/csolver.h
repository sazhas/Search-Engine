#ifndef CSOLVER_H
#define CSOLVER_H

#include <string>
#include <vector>

#include <netinet/in.h>

#include "../indexer/Indexer.hpp"   // for IndexBlob
#include "../ranker/Ranker.hpp"
#include "ast.h"

const uint32_t MAX_RESULTS = 10;
const uint32_t MAX_RANKED_DOCS = 200;

namespace Ranker {
class Ranker;
}

struct RankingResult;

class CSolver {
private:
    inline static CSolver* instance = nullptr;
    sockaddr_in addr_serv;
    int fd_serv;


    ~CSolver();
    CSolver(const std::string& ip, unsigned port, const std::vector<IndexBlob*>& blobs);


public:
    bool process_client_request(int fd_client);
    std::vector<IndexBlob*> blobs;
    static void init_instance(const std::string& ip, unsigned port, const std::vector<IndexBlob*>& blobs);
    static CSolver* get_instance();

    void serve_requests();

    static void serialize_results(int fd_client, const std::vector<RankingResult>& results);
};

#endif /* CSOLVER_H */
