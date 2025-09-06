#pragma once

#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <utility>
#include <vector>


using namespace std;

template <typename Key, typename Value>
class Tuple {
public:
    Key key;
    Value value;

    Tuple(const Key& k, const Value& v)
        : key(k)
        , value(v) {}

    Tuple(Key&& k, Value&& v)
        : key(std::move(k))
        , value(std::move(v)) {}

    Tuple(Tuple&& t)
        : Tuple(std::move(t.key), std::move(t.value)) {}
};

// **Bucket Class**
template <typename Key, typename Value>
class Bucket {
public:
    Bucket* next;
    uint32_t hashValue;
    Tuple<Key, Value> tuple;

    Bucket(const Key& k, uint32_t h, const Value v)
        : next(nullptr)
        , hashValue(h)
        , tuple(k, v) {}

    ~Bucket() { delete next; }
};

template <typename Key>
inline uint32_t HashFunction(const Key& k) {
    const char* str = reinterpret_cast<const char*>(k);
    uint32_t hash = 2166136261u;
    while (*str) {
        hash ^= static_cast<uint32_t>(*str);
        hash *= 16777619u;   // FNV-1a prime
        str++;
    }
    return hash;
}

template <typename Key>
uint32_t HashFunction(const Key& k, size_t numBuckets) {
    const char* str = reinterpret_cast<const char*>(k);
    uint32_t hash = 2166136261u;
    while (*str) {
        hash ^= static_cast<uint32_t>(*str);
        hash *= 16777619u;   // FNV-1a prime
        str++;
    }
    return hash % numBuckets;
}

// **HashTable Class**
template <typename Key, typename Value>
class HashTable {
private:
    Bucket<Key, Value>** buckets;
    size_t capacity;
    size_t size;
    double maxLoadFactor;
    size_t maxBucketSize;
    bool bucketSizeExceeded;
    std::vector<size_t> activeBuckets;

    friend class Iterator;
    friend class HashBlob;

    static bool KeyCompare(const Key& a, const Key& b) { return strcmp(a, b) == 0; }

    // **Rehashing strat
    void Rehash(double growthFactor = 1.75) {
        size_t newCapacity = static_cast<size_t>(capacity * growthFactor);
        Bucket<Key, Value>** newBuckets = new Bucket<Key, Value>* [newCapacity] {};

        for (size_t i = 0; i < capacity; i++) {
            Bucket<Key, Value>* cur = buckets[i];

            while (cur) {
                Bucket<Key, Value>* next = cur->next;
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

    // internal op, called in find() to trigger rehash when needed
    void OptimizeInternal() {
        double load = static_cast<double>(size) / static_cast<double>(capacity);
        if (load > maxLoadFactor || bucketSizeExceeded) {
            Rehash();   // olny rehash, don't store active buckets
        }
    }

public:
    // getter
    const std::vector<size_t>& GetActiveBuckets() const { return activeBuckets; }

    // getter
    Bucket<Key, Value>* GetBucket(size_t index) const { return buckets[index]; }

    Tuple<Key, Value>* Find(const Key k, const Value initialValue) {
        uint32_t rawHash = HashFunction(k);
        uint32_t hashIndex = rawHash % capacity;
        auto& bucket = buckets[hashIndex];

        Tuple<Key, Value>* result = nullptr;

        if (bucket == nullptr) {
            size++;
            bucket = new Bucket<Key, Value>(k, rawHash, initialValue);
            result = &bucket->tuple;
            OptimizeInternal();
        } else {
            size_t bucketDepth = 0;
            auto* cur = bucket;

            while (cur->hashValue != rawHash || !KeyCompare(cur->tuple.key, k)) {
                if (cur->next == nullptr) {
                    size++;
                    cur->next = new Bucket<Key, Value>(k, rawHash, initialValue);
                    cur = cur->next;
                    break;
                }
                cur = cur->next;
                bucketDepth++;
            }

            if (bucketDepth > maxBucketSize) {
                bucketSizeExceeded = true;
                OptimizeInternal();
            }

            result = &cur->tuple;
        }

        // print_and_return:
        //     // Dump all non-empty buckets
        //     std::cout << "\n==== Bucket Debug Dump ====\n";
        //     for (size_t i = 0; i < capacity; ++i) {
        //         auto* b = buckets[i];
        //         if (b != nullptr) {
        //             std::cout << "Bucket " << i << ": ";
        //             while (b) {
        //                 std::cout << b->tuple.key << " -> ";
        //                 b = b->next;
        //             }
        //             std::cout << "NULL\n";
        //         }
        //     }
        //     std::cout << "===========================\n";

        return result;
    }


    Tuple<Key, Value>* Find(const Key k) const {
        uint32_t rawHash = HashFunction(k);
        uint32_t hashIndex = rawHash % capacity;
        auto* bucket = buckets[hashIndex];

        while (bucket) {
            if (bucket->hashValue == rawHash && KeyCompare(bucket->tuple.key, k)) return &bucket->tuple;
            bucket = bucket->next;
        }
        return nullptr;
    }

    void Optimize() {
        Rehash();   // grow table if needed
        activeBuckets.clear();

        for (size_t i = 0; i < capacity; i++) {
            if (buckets[i] != nullptr) {
                activeBuckets.push_back(i);   // store index of non empty buckets
            }
        }
    }


    bool erase(const Key& key) {
        uint32_t rawHash = HashFunction(key);
        uint32_t hashIndex = rawHash % capacity;
        Bucket<Key, Value>* curr = buckets[hashIndex];
        Bucket<Key, Value>* prev = nullptr;
        while (curr) {
            if (curr->hashValue == rawHash && KeyCompare(curr->tuple.key, key)) {
                if (prev) {
                    prev->next = curr->next;
                } else {
                    // Removing the first bucket in this index.
                    buckets[hashIndex] = curr->next;
                }
                curr->next = nullptr;  // Disconnect the chain so recursive delete doesn't free further buckets.
                delete curr;
                size--;
                return true;
            }
            prev = curr;
            curr = curr->next;
        }
        return false;
    }
    
    size_t Size() const noexcept { return size; }

    HashTable(size_t initialCapacity = 8, double loadFactor = 2.0, size_t maxDepth = 64)
        : capacity(initialCapacity)
        , maxLoadFactor(loadFactor)
        , maxBucketSize(maxDepth) {
        buckets = new Bucket<Key, Value>* [capacity] {};
        bucketSizeExceeded = false;
        size = 0;
    }

    ~HashTable() {
        for (size_t i = 0; i < capacity; i++) {
            delete buckets[i];
        }
        delete[] buckets;
    }

    class Iterator {
    private:
        HashTable* table;
        Bucket<Key, Value>* node;
        size_t bucketIndex;

    public:
        Iterator(HashTable* ht, size_t index, Bucket<Key, Value>* b)
            : table(ht)
            , node(b)
            , bucketIndex(index) {}

        Iterator()
            : table(nullptr)
            , node(nullptr)
            , bucketIndex(0) {}

        Tuple<Key, Value>& operator*() { return node->tuple; }

        Tuple<Key, Value>* operator->() const { return node ? &node->tuple : nullptr; }

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
        while (index < capacity && !buckets[index]) {
            index++;
        }
        return index == capacity ? Iterator {} : Iterator { this, index, buckets[index] };
    }

    Iterator end() { return Iterator {}; }
};
