#include <algorithm>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <string>
#include <vector>

// Include your indexing-related headers.
#include "../../csolver/isr_copy.h"
#include "../Indexer.hpp"

// Replace the synthetic vocabulary with a list of real words.
std::vector<std::string> vocab
  = { "apple",      "banana",    "cherry", "date",      "elderberry", "fig",    "grape",  "honeydew",
      "kiwi",       "lemon",     "mango",  "nectarine", "orange",     "papaya", "quince", "raspberry",
      "strawberry", "tangerine", "ugli",   "vanilla",   "watermelon", "xigua",  "yam",    "zucchini" };

std::vector<std::string> commonWords = { "the", "and",  "or", "of", "to", "a",  "in", "that", "is",  "for",
                                         "on",  "with", "as", "by", "at", "an", "be", "this", "are", "from" };

// (Optional) If you need a function to generate random words from the above list:
std::string GetRandomWord(double pCommon = 0.3) {
    double r = static_cast<double>(rand()) / RAND_MAX;
    if (r < pCommon) {
        return commonWords[rand() % commonWords.size()];
    }
    return vocab[rand() % vocab.size()];
}

// Generate a vector of WFs (word/flag structures) for a document.
// We assume that WFs is defined in your Indexer code with at least members: word and flags.
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

    // (Vocabulary is now set to a list of real words; no need to generate 50,000 synthetic ones.)
    std::cout << "\n=== Using Real-Word Vocabulary ===\n";
    std::cout << "Vocabulary size: " << vocab.size() << "\n";

    // Create a vector of HTML parser pointers for documents.
    std::vector<HtmlParser*> toProcess;

    std::cout << "\n=== Generating 3000 Fake Documents ===\n";
    // (For quicker tests, we use fewer documents than 30000.)
    for (int i = 0; i < 3000; i++) {
        // Create a new HtmlParser document (assume appropriate constructor exists).
        HtmlParser* doc = new HtmlParser("", 0);
        // Assign a unique URL.
        doc->pageURL = "http://fake" + std::to_string(i) + ".com";

        // Generate title words using the real vocabulary.
        doc->titleWords = { vocab[rand() % vocab.size()], vocab[rand() % vocab.size()] };
        // Generate 500 words per document using our frequency-emulating function.
        doc->words_flags = GenerateWords(500);

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
    }

    std::cout << "\n=== Creating IndexBlob and Testing Basic Lookups ===\n";
    IndexBlob* blob = IndexBlob::Create(&index);
    if (!blob) {
        std::cerr << "IndexBlob creation failed!" << std::endl;
        return 1;
    }

    // Test lookups on 10 random words.
    for (int i = 0; i < 10; i++) {
        // Choose a random term (from commonWords or the real vocabulary).
        const char* term
          = (rand() % 2 == 0) ? commonWords[rand() % commonWords.size()].c_str() : vocab[rand() % vocab.size()].c_str();
        ISRWord* isr = blob->OpenISRWord(term);
        if (!isr) {
            std::cout << "[MISSING] Word: " << term << "\n";
        } else {
            std::cout << "[FOUND] Word: " << term << " with " << isr->GetPostCount() << " posts\n";
            delete isr;
        }
    }

    // --------------------------------------------------------------------
    // Composite ISR Testing
    // --------------------------------------------------------------------
    std::cout << "\n=== Testing Composite ISRs ===\n";

    // --- ISRPhrase ---
    // For a phrase search, we want two words to occur consecutively.
    // Here we choose "apple" followed by "banana" (assuming these appear in some docs).
    ISRWord* phraseWord1 = blob->OpenISRWord("apple");
    ISRWord* phraseWord2 = blob->OpenISRWord("banana");
    // Create ISRPhrase using these ISRWord children.
    // (Assume that ISRPhrase takes a vector<ISR*> as its constructor argument.)
    ISRPhrase* phraseISR = new ISRPhrase({ phraseWord1, phraseWord2 });
    if (phraseISR->Next()) {
        std::cout << "ISRPhrase: Found a document where 'apple banana' occur consecutively. "
                  << "Start=" << phraseISR->GetStartLocation() << ", End=" << phraseISR->GetEndLocation() << "\n";
    } else {
        std::cout << "ISRPhrase: No matching document found for 'apple banana'.\n";
    }
    // Delete composite and its children.
    delete phraseISR;
    // (Assume that the composite ISRs do not delete their children, so we delete them here if needed.)
    // (If they do, then remove the following lines.)
    delete phraseWord1;
    delete phraseWord2;

    // --- ISRAnd ---
    // For an AND search, we want documents that contain both "apple" and "cherry".
    ISRWord* andWord1 = blob->OpenISRWord("apple");
    ISRWord* andWord2 = blob->OpenISRWord("cherry");
    ISRAnd* andISR = new ISRAnd({ andWord1, andWord2 });
    if (andISR->Next()) {
        std::cout << "ISRAnd: Found a document containing both 'apple' and 'cherry'. "
                  << "Start=" << andISR->GetStartLocation() << ", End=" << andISR->GetEndLocation() << "\n";
    } else {
        std::cout << "ISRAnd: No matching document found for 'apple' AND 'cherry'.\n";
    }
    delete andISR;
    delete andWord1;
    delete andWord2;

    // --- ISROr ---
    // For an OR search, a document containing either "apple" or "date" is acceptable.
    ISRWord* orWord1 = blob->OpenISRWord("apple");
    ISRWord* orWord2 = blob->OpenISRWord("date");
    ISROr* orISR = new ISROr({ orWord1, orWord2 });
    if (orISR->Next()) {
        std::cout << "ISROr: Found a document containing 'apple' or 'date'. "
                  << "Start=" << orISR->GetStartLocation() << ", End=" << orISR->GetEndLocation() << "\n";
    } else {
        std::cout << "ISROr: No matching document found for 'apple' OR 'date'.\n";
    }
    delete orISR;
    // delete child ISRs if needed:
    delete orWord1;
    delete orWord2;

    // --- ISRContainer ---
    // For a container search, we include documents that contain both "apple" and "banana"
    // but exclude those that contain "grape".
    ISRWord* containerIncl1 = blob->OpenISRWord("apple");
    ISRWord* containerIncl2 = blob->OpenISRWord("banana");
    ISRWord* containerExcl = blob->OpenISRWord("nsidnidkcnd");
    ISRContainer* containerISR = new ISRContainer({ containerIncl1, containerIncl2 }, { containerExcl });
    if (containerISR->Next()) {
        std::cout << "ISRContainer: Found a document containing 'apple' and 'banana' but not 'grape'. "
                  << "Start=" << containerISR->GetStartLocation() << ", End=" << containerISR->GetEndLocation() << "\n";
    } else {
        std::cout << "ISRContainer: No matching document found for included/excluded search.\n";
    }
    delete containerISR;
    delete containerIncl1;
    delete containerIncl2;
    delete containerExcl;

    // --------------------------------------------------------------------
    // Save the index to file.
    // --------------------------------------------------------------------
    std::cout << "\n=== Saving Index to File ===\n";
    IndexFile file("test_index_large.bin", &index);
    std::cout << "Saved to test_index_large.bin\n";

    IndexBlob::Discard(blob);
    std::cout << "\n=== Testing Completed ===\n";
    return 0;
}
