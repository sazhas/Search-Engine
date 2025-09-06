#include <iostream>
#include <fstream>
#include <unordered_map>
#include <string>
#include "../HashTable.h"
#include "../algorithm.h"


int main() {
    std::ifstream infile("similar_urls.txt");
    std::string url;
    std::unordered_map<int, int> mod2, mod5;

    while (std::getline(infile, url)) {
        if (url.empty() || url[0] == '#') continue;

        uint32_t hash2 = hash_url(url.c_str()) % 2;
        uint32_t hash5 = hash_url(url.c_str()) % 6;

        mod2[hash2]++;
        mod5[hash5]++;
    }

    std::cout << "mod 2 distribution:\n";
    for (int i = 0; i < 2; ++i)
        std::cout << i << ": " << mod2[i] << '\n';

    std::cout << "\nmod 5 distribution:\n";
    for (int i = 0; i < 6; ++i)
        std::cout << i << ": " << mod5[i] << '\n';
}
