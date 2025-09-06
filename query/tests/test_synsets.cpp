#include "../../lib/stemmer.h"
#include "../synsets.h"
#include <iostream>
#include <string>

int main() {
    // Initialize the Synsets class with a test file
    Synsets::init("/home/alexycn/Downloads/synsets.txt");

    std::string str;
    while(std::getline(std::cin, str)){
        // Get the stem of the input string
        std::string stem = Stemmer::stem(str);

        // Get the synsets for the stem
        const auto* synsets = Synsets::get_synsets(stem);

        // If synsets are found, print them
        if (synsets) {
            std::cout << "Synsets for stem '" << stem << "':\n";
            for (const auto& synset : *synsets) {
                std::cout << "  - ";
                for (const auto& word : *synset) {
                    std::cout << word << "; ";
                };
                std::cout << "\n";
            }
        } else {
            std::cout << "No synsets found for stem '" << stem << "'\n";
        }
    }

    return 0;
}
