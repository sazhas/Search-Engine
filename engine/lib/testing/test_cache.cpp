#include <iostream>
#include <vector>
#include <string>
#include "../cache.h"  // Your updated LRU cache with HashTable implementation

using namespace std;

// Updated printCacheStatus that uses peek() to avoid changing recency.
void printCacheStatus(const char* label, LRUCache<const char*, std::vector<std::string>>& cache) {
    vector<string> result;
    cout << label << endl;

    // We check a set of predetermined keys.
    for (const char* key : {"A", "B", "C", "D", "E"}) {
        if (cache.peek(key, result)) {
            cout << "  " << key << ": [ ";
            for (const string& url : result) 
                cout << url << " ";
            cout << "]" << endl;
        } else {
            cout << "  " << key << ": <evicted>" << endl;
        }
    }
    cout << "----------------------" << endl;
}

int main() {
    // Create an LRU cache instance with capacity 3.
    LRUCache<const char*, std::vector<std::string>> cache(3);

    // Insert keys A, B, and C.
    cache.put("A", {"url1"});
    cache.put("B", {"url2"});
    cache.put("C", {"url3"});

    // Access A twice and B once.
    // Initially, inserting in order gives the list as: C (MRU) , B, A (LRU) because new keys are added to the front.
    // Calling get("A") moves A to the front, and then get("B") brings B to the front.
    // Thus, expected recency order becomes: B, A, C (with C being the least recently used).
    vector<std::string> temp;
    cache.get("A", temp);
    cache.get("A", temp);
    cache.get("B", temp);

    printCacheStatus("Before inserting D (expect C to be evicted on D insertion)", cache);

    // Insert D. Since the cache is full, the least recently used key (C) should be evicted.
    cache.put("D", {"url4"});
    printCacheStatus("After inserting D (C should be evicted)", cache);

    // Additional check: Access D multiple times to mark it as most-recently used.
    cache.get("D", temp);
    cache.get("D", temp);
    cache.get("D", temp);

    // Now insert E. With the current order, the least recently used key should now be A.
    cache.put("E", {"url5"});
    printCacheStatus("After inserting E (A should be evicted)", cache);

    return 0;
}
