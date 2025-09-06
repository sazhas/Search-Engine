#include "synsets.h"

void Synsets::init(const std::string& file) {
    std::ifstream in(file);
    if (!in) {
        throw std::runtime_error("Unable to open file: " + file);
    }

    std::string line;
    // TODO: cached + multithreaded loading
    while (std::getline(in, line)) {
        std::istringstream iss(line);
        Synset synset;
        std::string word;
        while (std::getline(iss, word, ';')) {
            synset.push_back(std::move(word));
        }
        synsets.push_back(std::move(synset));

        for (const auto& w : synsets.back()) {
            stem_to_synsets[Stemmer::stem(w)].push_back(&synsets.back());
        }
    }

    in.close();
}

const std::vector<const Synsets::Synset*>* Synsets::get_synsets(const std::string& stem) {
    auto it = stem_to_synsets.find(stem);

    if (it != stem_to_synsets.end()) {
        return &it->second;
    }
    
    else
        return nullptr;
}