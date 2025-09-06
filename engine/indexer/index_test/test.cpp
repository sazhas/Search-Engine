#include <chrono>
#include <iostream>
#include <string>
#include <vector>

#include "../Indexer.hpp"

void printWordISR(ISRWord* isr, const char* term) {
    if (!isr) {
        std::cout << "[MISSING] '" << term << "' not found in the index.\n";
        return;
    }

    std::cout << "[FOUND] Word '" << term << "' found with " << isr->GetPostCount() << " posts\n";

    // Print first few posts
    int count = 0;
    Post* post = isr->Next();
    while (post) {
        std::cout << "  Post " << count << ": Location=" << post->GetStartLocation()
                  << ", Flags=" << (int) ((WordPost*) post)->flags << "\n";
        count++;
        post = isr->Next();
    }
}

void printDocISR(ISRDoc* isr, const char* term) {
    if (!isr) {
        std::cout << "[MISSING] '" << term << "' not found in the index.\n";
        return;
    }

    std::cout << "[FOUND] Document '" << term << "' found with " << isr->GetPostCount() << " posts\n";

    // Print document details
    Post* post = isr->Next();
    if (post) {
        std::cout << "  Document: Start=" << post->GetStartLocation() << ", End=" << post->GetEndLocation()
                  << ", DocID=" << post->GetID() << "\n"
                  << "  Word Count: " << isr->GetWordCount() << ", Unique Words: "
                  << ", URL Length: " << isr->GetUrlLength() << "\n";
    }
}

int main() {
    Index index;

    // Create test documents with both title and body words
    HtmlParser* doc1 = new HtmlParser("", 0);
    doc1->pageURL = "http://test1.com";
    doc1->titleWords = { "motherfucking", "test" };
    doc1->words_flags = {
        {         "this", 0x00},
        {           "is", 0x01}, // bold
        {            "a", 0x02}, // heading
        {         "test", 0x03}, // bold + heading
        {"motherfucking", 0x04}, // large font
        {     "document", 0x07}  // all flags set
    };


    HtmlParser* doc2 = new HtmlParser("", 0);
    doc2->pageURL = "http://test2.com";
    doc2->titleWords = { "another", "test" };
    doc2->words_flags = {
        {"motherfucking", 0x00},
        {         "test", 0x01},
        {        "again", 0x02},
        {     "document", 0x05}  // bold + large font
    };

    // Insert documents into index
    std::vector<HtmlParser*> toProcess = { doc1, doc2 };
    std::cout << "\n=== Inserting Documents ===\n";
    for (HtmlParser* parser : toProcess) {
        if (parser) {
            index.Insert(parser);
            std::cout << "Inserted: " << parser->pageURL << "\n";
            delete parser;
        }
    }

    // Create index blob for searching
    auto blob = IndexBlob::Create(&index);

    // Test word ISRs
    std::cout << "\n=== Testing Word ISRs ===\n";
    std::vector<const char*> vocab = {
        "apple",    "banana",   "cat",      "dog",       "elephant",    "flower",  "grape",    "house",    "ice",
        "jungle",   "kite",     "lion",     "monkey",    "night",       "orange",  "pizza",    "queen",    "rain",
        "sun",      "tree",     "umbrella", "vase",      "whale",       "xray",    "yogurt",   "zebra",    "ant",
        "book",     "cup",      "desk",     "eagle",     "fish",        "gold",    "hat",      "ink",      "jar",
        "key",      "lamp",     "moon",     "nest",      "owl",         "pen",     "quilt",    "rock",     "star",
        "tiger",    "uniform",  "violin",   "window",    "xylophone",   "yarn",    "zoo",      "arch",     "ball",
        "cloud",    "doll",     "egg",      "fan",       "game",        "hill",    "iron",     "jewel",    "kite",
        "leaf",     "mountain", "net",      "ocean",     "pig",         "quiz",    "road",     "ship",     "tower",
        "unit",     "valley",   "wheel",    "xerox",     "yolk",        "zenith",  "actor",    "beach",    "candle",
        "drum",     "engine",   "forest",   "glove",     "hammer",      "igloo",   "jam",      "kangaroo", "ladder",
        "mirror",   "needle",   "orbit",    "pencil",    "quiet",       "robot",   "saddle",   "train",    "umpire",
        "vulture",  "wrench",   "xenon",    "yak",       "zinc",        "ankle",   "bracelet", "cookie",   "donut",
        "elbow",    "feather",  "guitar",   "hose",      "insect",      "jacket",  "kettle",   "laptop",   "magnet",
        "notebook", "oven",     "plant",    "quartz",    "remote",      "scale",   "tent",     "utensil",  "vaccine",
        "wallet",   "xbox",     "yeti",     "zipper",    "alley",       "badge",   "canvas",   "dart",     "estate",
        "ferry",    "grill",    "harbor",   "identity",  "jungle",      "karate",  "lantern",  "mango",    "noodle",
        "opera",    "parade",   "quill",    "racket",    "scooter",     "ticket",  "utility",  "vendor",   "waffle",
        "xmas",     "yawn",     "zombie",   "atlas",     "binary",      "crayon",  "dialog",   "echo",     "fig",
        "gazebo",   "hinge",    "insight",  "jigsaw",    "koala",       "locust",  "mural",    "nacho",    "oxygen",
        "plasma",   "quote",    "radius",   "syrup",     "toggle",      "urine",   "vortex",   "wok",      "xerus",
        "yodel",    "zeppelin", "anvil",    "brook",     "creek",       "daisy",   "embassy",  "fjord",    "grove",
        "hatch",    "index",    "jungle",   "knob",      "lichen",      "microbe", "nectar",   "obelisk",  "portal",
        "quest",    "raven",    "sphinx",   "tulip",     "uplift",      "vigil",   "warp",     "xylem",    "yeast",
        "zapper",   "amber",    "basin",    "cliff",     "dune",        "ember",   "forge",    "goblet",   "harp",
        "ivy",      "jester",   "kernel",   "lagoon",    "mantis",      "nugget",  "ore",      "pylon",    "quasar",
        "reef",     "silo",     "turret",   "usher",     "visor",       "wander",  "xeric",    "yonder",   "zebrawood",
        "arcade",   "breeze",   "cipher",   "dock",      "envy",        "fuse",    "glacier",  "hoop",     "icy",
        "javelin",  "keystone", "lizard",   "minnow",    "nymph",       "oracle",  "plume",    "quench",   "rust",
        "scythe",   "throne",   "unity",    "voxel",     "wrath",       "xiphoid", "yeti",     "zen",      "avalanche",
        "butter",   "clover",   "doodle",   "estate",    "fringe",      "groove",  "hearth",   "incense",  "jogger",
        "kernel",   "lampoon",  "meadow",   "niche",     "obelus",      "panther", "quaint",   "rover",    "sizzle",
        "thistle",  "uplink",   "vault",    "warden",    "xerothermic", "yammer",  "zircon",   "anchor",   "blizzard",
        "coral",    "delta",    "ember",    "falcon",    "gala",        "hazard",  "isle",     "jungle",   "karma",
        "lemon",    "mantra",   "nexus",    "oracle",    "prism",       "quota",   "relay",    "signal",   "tidal",
        "unicorn",  "verge",    "wind",     "xylephone", "yoga",        "zodiac",  "atom",     "blade",    "crook",
        "dune",     "element",  "flare",    "golem",     "haven",       "isotope", "jewel",    "knight",   "lava",
        "myst",     "node",     "omen",     "pulse",     "quorum",      "rift",    "shade",    "totem",    "urn",
        "vessel",   "whisper",  "xenial",   "youth",     "zenithal",    "and",     "the",      "a",        "or"
    };

    std::vector<const char*> wordTerms = { "motherfucking", "test", "document", "nonexistent" };
    for (const auto& term : wordTerms) {
        ISRWord* isr = blob->OpenISRWord(term);
        printWordISR(isr, term);
        delete isr;
    }

    // Test document ISRs
    std::cout << "\n=== Testing Document ISRs ===\n";
    ISRDoc* docIsr = blob->OpenISREndDoc();
    if (docIsr) {
        Post* doc = docIsr->Next();
        while (doc) {
            std::cout << "Document: Start=" << doc->GetStartLocation() << ", End=" << doc->GetEndLocation()
                      << ", DocID=" << doc->GetID() << "\n"
                      << "  Word Count: " << docIsr->GetWordCount() << ", URL Length: " << docIsr->GetUrlLength()
                      << "\n";
            doc = docIsr->Next();
        }
        delete docIsr;
    } else {
        std::cout << "No documents found in index\n";
    }

    // Test word occurrences in documents
    std::cout << "\n=== Testing Word Occurrences in Documents ===\n";
    ISRWord* wordIsr = blob->OpenISRWord("motherfucking");
    if (wordIsr) {
        ISRDoc* docIsr2 = blob->OpenISREndDoc();
        if (docIsr2) {
            std::cout << "Documents containing 'motherfucking': " << wordIsr->GetDocumentCount(docIsr2) << "\n";

            // Create a new ISRDoc for the second pass
            ISRDoc* docIsr3 = blob->OpenISREndDoc();
            if (docIsr3) {
                Post* doc = docIsr3->Next();
                while (doc) {
                    std::cout << "  Occurrences in doc " << doc->GetID() << ": "
                              << wordIsr->GetOccurrencesInCurrDoc(doc->GetStartLocation(), doc->GetEndLocation())
                              << "\n";
                    doc = docIsr3->Next();
                }
                delete docIsr3;
            }
            delete docIsr2;
        }
        delete wordIsr;
    }

    // Save to file and test loading
    std::cout << "\n=== Testing File I/O ===\n";
    IndexFile file("./test_index.bin", &index);
    std::cout << "Saved index to test_index.bin\n";

    std::cout << "\n=== Timing File Load and ISR Access ===\n";

    auto start = std::chrono::high_resolution_clock::now();

    IndexFile loadedFile("/Users/anaym/Documents/search engine/engine/indexer/index_test/index_chunk1.bin");

    auto mid = std::chrono::high_resolution_clock::now();

    std::cout << "\nLoaded index from file.\nNow printing ISRs:\n";

    for (const auto& term : vocab) {
        ISRWord* isr = loadedFile.blob->OpenISRWord(term);
        printWordISR(isr, term);
        delete isr;
    }

    auto end = std::chrono::high_resolution_clock::now();

    std::chrono::duration<double> loadTime = mid - start;
    std::chrono::duration<double> queryTime = end - mid;

    std::cout << "\n[Timing] File load time: " << loadTime.count() << " seconds\n";
    std::cout << "[Timing] ISR query time: " << queryTime.count() << " seconds\n";

    IndexBlob::Discard(blob);
    return 0;
}
