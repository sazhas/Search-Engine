#ifndef CACHE_H
#define CACHE_H

#pragma once
#include <list>
#include "HashTable.h"

// LRUCache: A templated Least Recently Used Cache for keys and values.
template <typename Key, typename Value>
class LRUCache {
public:
    // Constructor: initializes the cache with a fixed capacity.
    LRUCache(size_t capacity)
        : capacity_(capacity) {}

    // get: If the key exists, returns its value (by reference) in 'result' and moves the key to the front (most-recently used).
    // Returns true if the key was found.
    bool get(const Key& key, Value &result) {
        // Look for the key in the hash table that maps keys to list iterators.
        auto it = keyToIterator_.Find(key);
        if (!it)
            return false;
        // Move the corresponding node to the front of the list.
        cacheList_.splice(cacheList_.begin(), cacheList_, it->value);
        result = it->value->second;
        return true;
    }

    // put: Inserts a new key-value pair or updates an existing key.
    // If the cache is at capacity, it evicts the least recently used (LRU) key.
    void put(const Key& key, const Value &value) {
        if (capacity_ == 0)
            return;

        // Check if the key already exists in the cache.
        auto it = keyToIterator_.Find(key);
        if (it) {
            // Key exists: update its value and move it to the front.
            it->value->second = value;
            cacheList_.splice(cacheList_.begin(), cacheList_, it->value);
            return;
        }

        // If the cache is full, evict the least recently used item (back of the list).
        if (cacheList_.size() >= capacity_) {
            // The last element in the list is the LRU.
            auto lastIt = cacheList_.end();
            --lastIt;
            Key evictKey = lastIt->first;
            cacheList_.pop_back();
            keyToIterator_.erase(evictKey);
        }

        // Insert the new key-value pair at the front of the list.
        cacheList_.push_front(std::make_pair(key, value));
        // Insert the mapping from key to the list iterator in the hash table.
        keyToIterator_.Find(key, cacheList_.begin());
    }

    // In your LRUCache class, add this public method:
    bool peek(const Key& key, Value &result) const {
        // Note: This assumes your HashTable API allows a const Find() method.
        auto it = keyToIterator_.Find(key);  
        if (!it)
            return false;
        result = it->value->second;
        return true;
    }


private:
    // Define a type alias for the list iterator.
    typedef typename std::list<std::pair<Key, Value>>::iterator ListIt;
    size_t capacity_;
    // The list stores key-value pairs. The front of the list represents the most recently used item,
    // while the back represents the least recently used.
    std::list<std::pair<Key, Value>> cacheList_;
    // A hash table mapping keys to list iterators, for O(1) access and updates.
    HashTable<Key, ListIt> keyToIterator_;
};

#endif /* CACHE_H */

