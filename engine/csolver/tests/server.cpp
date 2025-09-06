#include <iostream>
#include "../csolver.h"

int main() {
    IndexFile file("bin/index_chunk0.bin");
    CSolver::init_instance("127.0.0.1", 9000, std::vector<IndexBlob*>{file.blob});
    CSolver* solver = CSolver::get_instance();
    solver->serve_requests();   
    return 0;
}