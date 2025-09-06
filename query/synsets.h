#ifndef SYNSETS_H
#define SYNSETS_H

#include <unordered_map>
#include <iostream>
#include <fstream>
#include <sstream>
#include <cstddef>
#include <deque>
#include <vector>
#include <string>

#include "../lib/stemmer.h"

class Synsets {
public:

    using Synset = std::vector<std::string>;

private:

    inline static std::deque<Synset> synsets;

    inline static std::unordered_map<std::string, std::vector<const Synset*>> stem_to_synsets;

public:

    static void init(const std::string& file);

    static const std::vector<const Synset*>* get_synsets(const std::string& stem);

}; /* class Synsets */

#endif // SYNSETS_H