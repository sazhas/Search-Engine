#include <algorithm>
#include <cstdlib>
#include <ctime>
#include <filesystem>   // Added for directory creation
#include <fstream>      // Added for file output
#include <iostream>
#include <string>
#include <vector>

// Include your indexing-related headers.
#include "../Indexer.hpp"

// Global containers for our vocabulary and common words.
std::vector<std::string> vocab;
std::vector<std::string> commonWords = { "the", "and",  "or", "of", "to", "a",  "in", "that", "is",  "for",
                                         "on",  "with", "as", "by", "at", "an", "be", "this", "are", "from" };

// Generate a large vocabulary. Here we generate numWords words: "word0", "word1", …
void GenerateVocab(size_t numWords) {
    vocab.reserve(numWords);
    for (size_t i = 0; i < numWords; i++) {
        vocab.push_back("word" + std::to_string(i));
    }
}

// Helper function to generate a random word token.
// With probability pCommon, returns a common word from the commonWords list;
// otherwise returns a random word from the large vocabulary.
std::string GetRandomWord(double pCommon = 0.3) {
    double r = (double) rand() / RAND_MAX;
    if (r < pCommon) {
        return commonWords[rand() % commonWords.size()];
    }
    return vocab[rand() % vocab.size()];
}

// Generate a vector of WFs (word/flag structures) for a document.
// This function now uses GetRandomWord so that common words appear more frequently.
std::vector<WFs> GenerateWords(int count) {
    std::vector<WFs> words;
    words.reserve(count);
    for (int i = 0; i < count; i++) {
        words.push_back({ GetRandomWord(), 0 });
    }
    return words;
}

int main() {
    // Seed the random generator.
    srand(static_cast<unsigned int>(time(nullptr)));

    // Expand our vocabulary to 50,000 words.
    GenerateVocab(50000);

    // Create a vector of HTML parser pointers for documents.
    std::vector<HtmlParser*> toProcess;

    std::cout << "\n=== Generating 10 Fake Documents ===\n";

    // Ensure output directory exists
    std::filesystem::create_directories("output");

    /*for (int i = 0; i < 10; i++) {
        // Create a new HtmlParser document (assume appropriate constructor exists).
        HtmlParser* doc = new HtmlParser("", 0);
        // Assign a unique URL.
        doc->pageURL = "http://fake" + std::to_string(i) + ".com";

        // Generate title words (using vocabulary – you could choose common words here too if desired).
        doc->titleWords = { vocab[rand() % vocab.size()], vocab[rand() % vocab.size()] };
        // Generate 500 words per document using our frequency-emulating function.
        doc->words_flags = GenerateWords(500);

        // Save document to its own text file
        std::ofstream docOut("output/doc_" + std::to_string(i) + ".txt");
        if (docOut) {
            docOut << "Document #" << i + 1 << "\n";
            docOut << "URL: " << doc->pageURL << "\n";
            docOut << "Title: " << doc->titleWords[0] << " " << doc->titleWords[1] << "\n";
            docOut << "Body: ";
            for (const auto& wf : doc->words_flags) {
                docOut << wf.word << " ";
            }
            docOut << "\n";
            docOut.close();
        } else {
            std::cerr << "Failed to open output/doc_" << i << ".txt for writing.\n";
        }

        toProcess.push_back(doc);
    }

    // Create the index.
    Index index;
    std::cout << "\n=== Inserting into Index ===\n";
    for (HtmlParser* parser : toProcess) {
        index.Insert(parser);
        if ((rand() % 1000) == 0) {   // Print occasionally for progress.
            std::cout << "Inserted: " << parser->pageURL << "\n";
        }
        delete parser;
    }*/

    std::cout << "\n=== Creating IndexBlob and Testing Lookups ===\n";
    IndexFile file("/Users/abelthomasnoble/Desktop/eecs-498/engine/indexer/index_test/output/test_index_large.bin");
    auto blob = file.blob;

    // Test lookups on 10 random words.
    for (int i = 0; i < 10; i++) {
        std::string term;
        std::cin >> term;
        ISRWord* isr = blob->OpenISRWord(term.c_str());
        if (!isr) {
            std::cout << "[MISSING] Word: " << term << "\n";
        } else {
            std::cout << "[FOUND] Word: " << term << " with " << isr->GetPostCount() << " posts\n";
            while (isr->Next()) {
                auto post = isr->GetCurrentDoc();
                std::cout << blob->GetURL(post->GetID()) << std::endl;
            }
            delete isr;
        }
    }

    std::cout << "\n=== Saving Index to File ===\n";
    std::cout << "Saved to output/test_index_large.bin\n";

    return 0;
}
