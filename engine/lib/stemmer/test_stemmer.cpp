#include <iostream>

#include "../stemmer.h"

int main() {
    std::string word;
    
    while(std::cin >> word) {
        std::cout << Stemmer::stem(word) << std::endl;
    }

    return 0;
}