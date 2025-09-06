#include <dirent.h>
#include <ostream>
#include <unordered_set>
#include <vector>
#include <string>
#include "../Indexer.hpp"

void list_bin_files(std::vector<std::string>& out) {
    DIR* dp = opendir(".");

    while (auto* ent = readdir(dp)) {
        std::string name = ent->d_name;
        if (name == "." || name == "..") continue;

        if (ent->d_type != DT_REG) continue;   // skip dirs etc.

        if (name.size() >= 4 && name.substr(name.size() - 4) == ".bin") {
            out.push_back("./" + name);
        }
    }
    closedir(dp);
}

int main() {
    std::vector<std::string> paths;
    list_bin_files(paths);

    Location total = 0;
    std::unordered_set<std::string> unique;

    for (const auto& path : paths) {
        IndexFile file(path.c_str());
        total += file.blob->WordsInIndex;
        auto hash_blob = file.blob->GetHashBlob();

        for (uint32_t i = 0; i < hash_blob->NumberOfBuckets; ++i) {
            auto offset = hash_blob->Buckets[i];
            const char* ptr = reinterpret_cast<const char*>(hash_blob) + offset;
            const SerialTuple* tuple = reinterpret_cast<const SerialTuple*>(ptr);
            while (tuple->Length != 0) {
                unique.insert(tuple->Key);
                ptr += tuple->Length;
                tuple = reinterpret_cast<const SerialTuple*>(ptr);
            }
        }
    }

    for (auto& word: unique) {
        std::cout << word << std::endl;
    }
    std::cout << "Total: " << total << std::endl;
    std::cout << "Unique: " << unique.size() << std::endl;
}
