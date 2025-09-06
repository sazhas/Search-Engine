#pragma once

#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <vector>

using namespace std;

// Bucket for UnorderedSet.
template <typename Key>
class SetBucket {
public:
    SetBucket* next;
    uint32_t hashValue;
    Key key;

    SetBucket(const Key& k, uint32_t h)
        : next(nullptr)
        , hashValue(h)
        , key(k) {}

    ~SetBucket() { delete next; }
};

// UnorderedSet implementation.
template <typename Key>
class UnorderedSet {
private:
    SetBucket<Key>** buckets;
    size_t capacity;
    size_t size;
    double maxLoadFactor;
    size_t maxBucketSize;
    bool bucketSizeExceeded;

    // FNV-1a hash function.
    static uint32_t HashFunction(const Key& k) {
        // Assumes Key is a C-string (const char*)
        const char* str = reinterpret_cast<const char*>(k);
        uint32_t hash = 2166136261u;
        while (*str) {
            hash ^= static_cast<uint32_t>(*str);
            hash *= 16777619u;
            str++;
        }
        return hash;
    }

    // Rehash into new bucket space given a growth factor.
    void Rehash(double growthFactor = 1.75) {
        size_t newCapacity = static_cast<size_t>(capacity * growthFactor);
        SetBucket<Key>** newBuckets = new SetBucket<Key>* [newCapacity] {};
        for (size_t i = 0; i < capacity; i++) {
            SetBucket<Key>* cur = buckets[i];
            while (cur) {
                SetBucket<Key>* next = cur->next;
                uint32_t newIndex = cur->hashValue % newCapacity;
                cur->next = newBuckets[newIndex];
                newBuckets[newIndex] = cur;
                cur = next;
            }
        }
        delete[] buckets;
        buckets = newBuckets;
        capacity = newCapacity;
        bucketSizeExceeded = false;
    }

    static bool KeyCompare(const Key& a, const Key& b) { return strcmp(a, b) == 0; }

    // Check if we need to optimize internal state.
    void OptimizeInternal() {
        double load = static_cast<double>(size) / static_cast<double>(capacity);
        if (load > maxLoadFactor || bucketSizeExceeded) {
            Rehash();
        }
    }

public:
    // Reserve space (i.e. preallocate bucket array) if newCapacity exceeds current capacity.
    void reserve(size_t newCapacity) {
        if (newCapacity > capacity) {
            double growthFactor = static_cast<double>(newCapacity) / capacity;
            Rehash(growthFactor);
        }
    }

    // Insert a key. Returns false if key already exists.
    bool Insert(const Key& key) {
        uint32_t rawHash = HashFunction(key);
        uint32_t hashIndex = rawHash % capacity;
        auto& bucket = buckets[hashIndex];
        if (bucket == nullptr) {
            bucket = new SetBucket<Key>(key, rawHash);
            size++;
            OptimizeInternal();
            return true;
        }
        SetBucket<Key>* cur = bucket;
        size_t depth = 0;
        while (true) {
            if (cur->hashValue == rawHash && KeyCompare(cur->key, key)) {
                return false;   // already exists
            }
            if (cur->next == nullptr) {
                cur->next = new SetBucket<Key>(key, rawHash);
                size++;
                if (depth > maxBucketSize) {
                    bucketSizeExceeded = true;
                    OptimizeInternal();
                }
                return true;
            }
            cur = cur->next;
            depth++;
        }
    }

    // Check if the key exists.
    bool Contains(const Key& key) const {
        uint32_t rawHash = HashFunction(key);
        uint32_t hashIndex = rawHash % capacity;
        SetBucket<Key>* bucket = buckets[hashIndex];
        while (bucket) {
            if (bucket->hashValue == rawHash && KeyCompare(bucket->key, key)) return true;
            bucket = bucket->next;
        }
        return false;
    }

    // Remove the key from the set.
    bool Erase(const Key& key) {
        uint32_t rawHash = HashFunction(key);
        uint32_t hashIndex = rawHash % capacity;
        SetBucket<Key>* curr = buckets[hashIndex];
        SetBucket<Key>* prev = nullptr;
        while (curr) {
            if (curr->hashValue == rawHash && KeyCompare(curr->key, key)) {
                if (prev)
                    prev->next = curr->next;
                else
                    buckets[hashIndex] = curr->next;
                curr->next = nullptr;
                delete curr;
                size--;
                return true;
            }
            prev = curr;
            curr = curr->next;
        }
        return false;
    }

    // Return the number of items in the set.
    size_t Size() const noexcept { return size; }

    // Constructor.
    UnorderedSet(size_t initialCapacity = 8, double loadFactor = 2.0, size_t maxDepth = 64)
        : capacity(initialCapacity)
        , size(0)
        , maxLoadFactor(loadFactor)
        , maxBucketSize(maxDepth)
        , bucketSizeExceeded(false) {
        buckets = new SetBucket<Key>* [capacity] {};
    }

    // Destructor.
    ~UnorderedSet() {
        for (size_t i = 0; i < capacity; i++) {
            delete buckets[i];
        }
        delete[] buckets;
    }

    // Iterator support.
    class Iterator {
    private:
        UnorderedSet* table;
        SetBucket<Key>* node;
        size_t bucketIndex;

    public:
        Iterator(UnorderedSet* set, size_t index, SetBucket<Key>* b)
            : table(set)
            , node(b)
            , bucketIndex(index) {}

        Iterator()
            : table(nullptr)
            , node(nullptr)
            , bucketIndex(0) {}

        const Key& operator*() const { return node->key; }
        const Key* operator->() const { return &(node->key); }

        Iterator& operator++() {
            if (node->next) {
                node = node->next;
                return *this;
            }
            while (++bucketIndex < table->capacity) {
                if (table->buckets[bucketIndex]) {
                    node = table->buckets[bucketIndex];
                    return *this;
                }
            }
            node = nullptr;
            return *this;
        }

        bool operator==(const Iterator& rhs) const { return this->node == rhs.node; }
        bool operator!=(const Iterator& rhs) const { return this->node != rhs.node; }
    };

    Iterator begin() {
        size_t index = 0;
        while (index < capacity && !buckets[index]) index++;
        return index == capacity ? Iterator {} : Iterator { this, index, buckets[index] };
    }

    Iterator end() { return Iterator {}; }
};
