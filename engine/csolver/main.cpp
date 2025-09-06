#include <cerrno>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include <dirent.h>
#include <unistd.h>

#include <sys/mman.h>

#include "../indexer/Indexer.hpp"   // IndexFile / IndexBlob
#include "csolver.h"

/* ---------------------------------------------------------
 *  Push every regular file that ends in ".bin" found in
 *  the *single* directory  <root>  into  out.
 * --------------------------------------------------------*/
static void list_bin_files(const std::string& root, std::vector<std::string>& out) {
    DIR* dp = opendir(root.c_str());
    if (!dp) throw std::runtime_error("cannot open " + root);

    while (auto* ent = readdir(dp)) {
        std::string name = ent->d_name;
        if (name == "." || name == "..") continue;

        if (ent->d_type != DT_REG) continue;   // skip dirs etc.

        if (name.size() >= 4 && name.substr(name.size() - 4) == ".bin") {
            out.push_back(root + '/' + name);
        }
    }
    closedir(dp);
}
constexpr size_t MAX_LOCK_BYTES = 40L * 1024 * 1024 * 1024;

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "usage: " << argv[0] << " <index‑root‑dir> [port]\n";
        return 1;
    }
    const std::string rootDir = argv[1];
    const unsigned port = (argc >= 3) ? std::stoi(argv[2]) : 8080;
    size_t total_locked_bytes = 0;

    std::vector<std::string> bin_paths;
    try {
        list_bin_files(rootDir, bin_paths);
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 2;
    }

    if (bin_paths.empty()) {
        std::cerr << "No *.bin files found directly under " << rootDir << '\n';
        return 3;
    }

    std::vector<IndexBlob*> blobs;
    std::vector<IndexFile> files;
    files.reserve(bin_paths.size());

    for (const auto& p : bin_paths) {
        try {
            files.emplace_back(p.c_str());
            IndexFile& idxFile = files.back();
            IndexBlob* blob = idxFile.blob;

            // Hint to OS to prefetch the mapped file
            if (madvise(blob, idxFile.Size(), MADV_WILLNEED) != 0) {
                std::cerr << "madvise failed for " << p << ": " << strerror(errno) << '\n';
            }

            // Only lock memory if we're under the limit
            if (total_locked_bytes + idxFile.Size() <= MAX_LOCK_BYTES) {
#ifdef MLOCK_ONFAULT
                if (mlock2(blob, idxFile.Size(), MLOCK_ONFAULT) != 0) {
                    if (errno == ENOSYS || errno == EINVAL) {
                        if (mlock(blob, idxFile.Size()) != 0) {
                            std::cerr << "mlock fallback failed for " << p << ": " << strerror(errno) << '\n';
                        }
                    } else {
                        std::cerr << "mlock2 failed for " << p << ": " << strerror(errno) << '\n';
                    }
                }
#else
                if (mlock(blob, idxFile.Size()) != 0) {
                    std::cerr << "mlock failed for " << p << ": " << strerror(errno) << '\n';
                }
#endif
                total_locked_bytes += idxFile.Size();
                std::cout << "Loaded and locked " << p << '\n';
            } else {
                std::cout << "Loaded without lock (RAM limit): " << p << '\n';
            }

            blobs.push_back(blob);

        } catch (const std::exception& e) {
            std::cerr << "Skipping " << p << " (" << e.what() << ")\n";
        }
    }

    if (blobs.empty()) {
        std::cerr << "All *.bin files failed to load.\n";
        return 4;
    }

    CSolver::init_instance("", port, blobs);
    CSolver::get_instance()->serve_requests();
    return 0;
}
