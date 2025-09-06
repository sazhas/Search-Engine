#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <string>
#include <vector>

#include <pthread.h>

// Include your indexing-related headers
#include "../Indexer.hpp"

// Global containers for our vocabulary and common words
std::vector<std::string> vocab;
std::vector<std::string> commonWords = { "the", "and",  "or", "of", "to", "a",  "in", "that", "is",  "for",
                                         "on",  "with", "as", "by", "at", "an", "be", "this", "are", "from" };

// Mutex for printing to console safely
pthread_mutex_t print_mutex = PTHREAD_MUTEX_INITIALIZER;

// Atomic counter for tracking progress
std::atomic<int> docs_inserted(0);

// Shared index that will be accessed by multiple threads
Index* shared_index;

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

// Generate a single document
HtmlParser* GenerateDocument(int id) {
    // Create a new HtmlParser document
    HtmlParser* doc = new HtmlParser("", 0);
    // Assign a unique URL
    doc->pageURL = "http://fake" + std::to_string(id) + ".com";

    // Generate title words
    doc->titleWords = { vocab[rand() % vocab.size()], vocab[rand() % vocab.size()] };
    // Generate 500 words per document using our frequency-emulating function
    doc->words_flags = GenerateWords(500);

    return doc;
}

// Thread function to process a batch of documents
void* ProcessDocuments(void* arg) {
    // Get the thread ID
    long thread_id = (long) arg;

    // Each thread will process a portion of the documents
    int total_docs = 30000;
    int num_threads = 100;
    int docs_per_thread = total_docs / num_threads;
    int start = thread_id * docs_per_thread;
    int end = (thread_id == num_threads - 1) ? total_docs : start + docs_per_thread;

    // Seed for this thread (use thread_id to ensure different seeds)
    unsigned int seed = static_cast<unsigned int>(time(nullptr)) + thread_id;
    srand(seed);

    // Process documents in this thread's range
    for (int i = start; i < end; i++) {
        HtmlParser* doc = GenerateDocument(i);

        // Insert into the shared index - this is what we're testing for thread safety
        shared_index->Insert(doc);

        // Increment the counter and occasionally print progress
        int current = ++docs_inserted;
        if (current % 1000 == 0) {
            pthread_mutex_lock(&print_mutex);
            std::cout << "Progress: " << current << "/" << total_docs << " documents inserted\n";
            pthread_mutex_unlock(&print_mutex);
        }

        delete doc;
    }

    pthread_mutex_lock(&print_mutex);
    std::cout << "Thread " << thread_id << " finished processing " << (end - start) << " documents\n";
    pthread_mutex_unlock(&print_mutex);

    return nullptr;
}

int main() {
    // Seed the random generator for the main thread
    srand(static_cast<unsigned int>(time(nullptr)));

    // Expand our vocabulary to 50,000 words
    std::cout << "Generating vocabulary...\n";
    GenerateVocab(50000);

    // Create the shared index
    shared_index = new Index();

    // Create threads
    pthread_t threads[100];
    std::cout << "\n=== Starting 8 threads to insert 30,000 documents ===\n";

    auto start_time = std::chrono::high_resolution_clock::now();

    // Launch threads
    for (long i = 0; i < 100; i++) {
        int result = pthread_create(&threads[i], nullptr, ProcessDocuments, (void*) i);
        if (result != 0) {
            std::cerr << "Error creating thread " << i << ": " << result << std::endl;
            return 1;
        }
    }

    // Wait for all threads to complete
    for (int i = 0; i < 100; i++) {
        pthread_join(threads[i], nullptr);
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::seconds>(end_time - start_time).count();

    std::cout << "\n=== All threads completed ===\n";
    std::cout << "Total documents inserted: " << docs_inserted << "\n";
    std::cout << "Time taken: " << duration << " seconds\n";

    std::cout << "\n=== Creating IndexBlob and Testing Lookups ===\n";
    auto blob = IndexBlob::Create(shared_index);

    // Test lookups on 10 random words
    for (int i = 0; i < 10; i++) {
        // Choose a random term (could be common or from the large vocabulary)
        const char* term
          = (rand() % 2 == 0) ? commonWords[rand() % commonWords.size()].c_str() : vocab[rand() % vocab.size()].c_str();

        ISRWord* isr = blob->OpenISRWord(term);
        if (!isr) {
            std::cout << "[MISSING] Word: " << term << "\n";
        } else {
            std::cout << "[FOUND] Word: " << term << " with " << isr->GetPostCount() << " posts\n";

            // Test navigation through the postings
            int count = 0;
            Post* post = isr->Next();   // Now explicitly call Next() to start navigation
            while (post && count < 5) {
                std::cout << "  Post " << count << ": Doc ID=" << post->GetID()
                          << ", Location=" << post->GetStartLocation() << "\n";
                post = isr->Next();
                count++;
            }

            delete isr;
        }
    }

    std::cout << "\n=== Saving Index to File ===\n";
    IndexFile file("test_index_multithreaded.bin", shared_index);
    std::cout << "Saved to test_index_multithreaded.bin\n";

    IndexBlob::Discard(blob);
    delete shared_index;

    pthread_mutex_destroy(&print_mutex);

    return 0;
}