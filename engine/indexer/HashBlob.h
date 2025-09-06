#pragma once

// HashBlob, a serialization of a HashTable into one contiguous
// block of memory, possibly memory-mapped to a HashFile.
//
// Nicole Hamilton  nham@umich.edu

#include <cassert>
#include <cstdint>
#include <cstring>

#include <fcntl.h>
#include <stdlib.h>

#include <sys/stat.h>
#include <sys/types.h>
// #include <malloc.h>
#include <cstdio>    // for perror
#include <cstdlib>   // for exit
#include <iostream>

#include <unistd.h>

#include <sys/mman.h>

#include "../lib/HashTable.h"
#include "Posts.hpp"

enum class TLD : uint8_t { UNKNOWN, GOV, EDU, ORG, COM, NET, IO, INFO, BIZ, XYZ, TOP, US, DEV };

inline TLD ParseTLD(const char* url) {
    const char* start = strstr(url, "://");
    if (!start) return TLD::UNKNOWN;

    start += 3;   // skip "://"
    const char* end = strchr(start, '/');
    std::string host = end ? std::string(start, end) : std::string(start);

    const char* lastDot = strrchr(host.c_str(), '.');
    if (!lastDot) return TLD::UNKNOWN;

    if (strcmp(lastDot, ".gov") == 0) return TLD::GOV;
    if (strcmp(lastDot, ".edu") == 0) return TLD::EDU;
    if (strcmp(lastDot, ".org") == 0) return TLD::ORG;
    if (strcmp(lastDot, ".com") == 0) return TLD::COM;
    if (strcmp(lastDot, ".net") == 0) return TLD::NET;
    if (strcmp(lastDot, ".io") == 0) return TLD::IO;
    if (strcmp(lastDot, ".info") == 0) return TLD::INFO;
    if (strcmp(lastDot, ".biz") == 0) return TLD::BIZ;
    if (strcmp(lastDot, ".xyz") == 0) return TLD::XYZ;
    if (strcmp(lastDot, ".top") == 0) return TLD::TOP;
    if (strcmp(lastDot, ".us") == 0) return TLD::US;
    if (strcmp(lastDot, ".dev") == 0) return TLD::DEV;

    return TLD::UNKNOWN;
}

// --------------------------------------------------------------------
// Document Attributes
// --------------------------------------------------------------------
struct DocumentAttributes {
    const char* url;          // URL of the document
    const char* title;        // Title of document
    uint32_t wordCount;       // Number of words in document
    uint32_t urlLength;       // URL length
    uint32_t titleLength;     // Title length in words
    uint32_t startLocation;   // Start location
    uint32_t endLocation;     // End Location
    bool english;
    uint8_t TLD;   // Score based on TLD

    DocumentAttributes()
        : url(nullptr)
        , title(nullptr)
        , wordCount(0)
        , urlLength(0)
        , titleLength(0)
        , startLocation(0)
        , endLocation(0)
        , english(true)
        , TLD(static_cast<uint8_t>(TLD::UNKNOWN)) {}

    DocumentAttributes(const char* u)
        : url(u)
        , title(nullptr)
        , wordCount(0)
        , urlLength(0)
        , titleLength(0)
        , startLocation(0)
        , endLocation(0)
        , english(true)
        , TLD(static_cast<uint8_t>(ParseTLD(u))) {}
};

// --------------------------------------------------------------------
// URL Table
// --------------------------------------------------------------------
struct URLTable {
    ~URLTable() {
        for (auto it : docAttributes) {
            free((void*) it.url);
            free((void*) it.title);
        }
    }
    uint32_t AddURL(const char* url) {
        uint32_t id = docAttributes.size();
        auto entry = urlsToID.Find(url, id);

        if (entry->value == id) {
            docAttributes.push_back(DocumentAttributes(url));
        } else {
            free((void*) url);
        }

        return entry->value;
    }

    const char* GetURL(uint32_t urlId) const {
        if (urlId < docAttributes.size()) {
            return docAttributes[urlId].url;
        }
        return "\0";
    }

    // Document attributes getter and setter
    void SetDocumentAttributes(const char* title, uint32_t urlId, uint32_t wordCount, uint32_t urlLength,
                               uint32_t titleLength, uint32_t startLocation, uint32_t endLocation, bool english) {
        if (urlId < docAttributes.size()) {
            auto& attrs = docAttributes[urlId];
            attrs.wordCount = wordCount;
            attrs.urlLength = urlLength;
            attrs.titleLength = titleLength;
            if (attrs.title) {
                free((void*) attrs.title);
            }
            attrs.title = title;
            attrs.startLocation = startLocation;
            attrs.endLocation = endLocation;
            attrs.english = english;
        }
    }

    const DocumentAttributes* GetDocumentAttributes(uint32_t urlId) const {
        return urlId < docAttributes.size() ? &docAttributes[urlId] : nullptr;
    }

    HashTable<const char*, uint32_t> urlsToID;
    std::vector<DocumentAttributes> docAttributes;
};

using Hash = HashTable<const char*, PostingList*>;
using Pair = Tuple<const char*, PostingList*>;
// A HashBucket is one chain (a linked list) stored in a Bucket node.

using HashBucket = Bucket<const char*, PostingList*>;

static const uint32_t Unknown = 0;

///////////////////////////////////////////////////////////////////////////////
// SerialTuple
///////////////////////////////////////////////////////////////////////////////
//
// Each SerialTuple represents a serialized record for one node in a
// hash bucket (linked list) from the HashTable. It contains:
//   - Length: total (aligned) length of this record (Length == 0 marks the end)
//   - Value: the stored value (uint32_t)
//   - HashValue: the precomputed hash (uint32_t)
//   - Key: a flexible array member that holds the C-string key (including its null terminator)
//
struct SerialTuple {
public:
    uint32_t Length;      // Total record length (nonzero) or zero for the sentinel.
    uint32_t Value;       // Offset to the SerializedPostingList from start of this SerialTuple
    uint32_t HashValue;   // Precomputed hash.
    char Key[Unknown];    // Flexible array member for the key.

    // Calculate the bytes required to encode an entire bucket chain.
    static uint32_t BytesRequired(const HashBucket* b) {
        uint32_t total = 0;
        for (const HashBucket* node = b; node != nullptr; node = node->next) {
            uint32_t keyLen = strlen(node->tuple.key) + 1;   // include null terminator

            // Calculate base structure size
            uint32_t baseSize = sizeof(uint32_t)   // Length field
                              + sizeof(uint32_t)   // Value field (now an offset)
                              + sizeof(uint32_t)   // HashValue field
                              + keyLen;            // key (with null terminator)

            // Round up the base structure size for alignment
            baseSize = RoundUp(baseSize, sizeof(uint32_t));

            // Add space for the actual posting list data
            uint32_t totalSize = baseSize + SerializedPostingList::BytesRequired(*node->tuple.value);

            // Round up the total size to maintain alignment for the next structure
            totalSize = RoundUp(totalSize, sizeof(uint32_t));

            total += totalSize;
        }

        // Add space for the sentinel record (with Length == 0)
        uint32_t sentinel = RoundUp(sizeof(uint32_t) + sizeof(uint32_t) + sizeof(uint32_t), sizeof(uint32_t));
        total += sentinel;

        return total;
    }

    // Write the entire bucket chain into the provided buffer.
    // Returns a pointer to one past the last byte written.
    static char* Write(char* buffer, const HashBucket* b) {
        for (const HashBucket* node = b; node != nullptr; node = node->next) {
            uint32_t keyLen = strlen(node->tuple.key) + 1;   // include null terminator
            uint32_t recordSize = sizeof(uint32_t)           // Length
                                + sizeof(uint32_t)           // Value (now an offset)
                                + sizeof(uint32_t)           // HashValue
                                + keyLen;                    // Key

            // Round up the base structure size
            uint32_t baseSize = RoundUp(recordSize, sizeof(uint32_t));

            // Add space for the actual posting list data
            uint32_t totalSize = baseSize + SerializedPostingList::BytesRequired(*node->tuple.value);

            // Round up the total size
            totalSize = RoundUp(totalSize, sizeof(uint32_t));

            SerialTuple* st = reinterpret_cast<SerialTuple*>(buffer);
            st->Length = totalSize;   // Mark as valid record with total size
            st->HashValue = node->hashValue;

            // Copy the key including its null terminator
            memcpy(st->Key, node->tuple.key, keyLen);
            // Explicitly ensure the key is null terminated
            st->Key[keyLen - 1] = '\0';

            // Calculate pointer to where the posting list data should be stored
            // (after the SerialTuple structure including the variable-length key)
            uint8_t* postingListLocation = reinterpret_cast<uint8_t*>(buffer + baseSize);

            // Copy the posting list data to the calculated location
            if (keyLen > 1) {
                SerializedPostingList::WriteWordPostingList(postingListLocation, *node->tuple.value);
            } else {
                SerializedPostingList::WriteDocumentPostingList(postingListLocation, *node->tuple.value);
            }

            // Store the offset to the posting list relative to the start of this SerialTuple
            st->Value = baseSize;

            buffer += totalSize;
        }

        // Write the sentinel record (Length == 0) to mark end of the chain
        uint32_t sentinel = RoundUp(sizeof(uint32_t) + sizeof(uint32_t) + sizeof(uint32_t), sizeof(uint32_t));
        SerialTuple* st = reinterpret_cast<SerialTuple*>(buffer);
        st->Length = 0;
        buffer += sentinel;
        return buffer;
    }
};

///////////////////////////////////////////////////////////////////////////////
// HashBlob
///////////////////////////////////////////////////////////////////////////////
//
// HashBlob is a contiguous serialization of an entire HashTable.
// Its layout is:
//
// [ Header ]
//   MagicNumber       (uint32_t)
//   Version           (uint32_t)
//   BlobSize          (uint32_t)
//   NumberOfBuckets   (uint32_t)
//   Buckets[]         (array of uint32_t offsets, one per bucket)
// [ Serialized Tuples ]
//   For each bucket (in order 0..NumberOfBuckets-1):
//     if bucket nonempty, the offset is set and the serialized SerialTuple
//     records for that bucket are stored (terminated by a sentinel record).
//
class HashBlob {
public:
    uint32_t MagicNumber;        // Magic number for validation.
    uint32_t Version;            // Format version.
    uint32_t BlobSize;           // Total size of the blob.
    uint32_t NumberOfBuckets;    // Number of buckets (should equal hash table capacity).
    uint32_t Buckets[Unknown];   // Array of offsets (one per bucket).

    // Look up a key in the blob. Returns pointer to the matching SerialTuple or nullptr.
    const SerialTuple* Find(const char* key) const {
        uint32_t hash = HashFunction(key, NumberOfBuckets);
        uint32_t offset = Buckets[hash];
        if (offset == 0) return nullptr;   // Empty bucket.
        const char* ptr = reinterpret_cast<const char*>(this) + offset;
        const SerialTuple* st = reinterpret_cast<const SerialTuple*>(ptr);
        while (st->Length != 0) {
            if (strcmp(st->Key, key) == 0) return st;
            ptr += st->Length;
            st = reinterpret_cast<const SerialTuple*>(ptr);
        }
        return nullptr;
    }

    // Calculate the total number of bytes required to serialize the hash table.
    static uint32_t BytesRequired(const Hash* hashTable) {
        // Assume that the hash table's capacity is accessible.
        uint32_t numBuckets = hashTable->capacity;
        // Header: 4 uint32_t fields + one uint32_t per bucket.
        uint32_t headerSize = (4 + numBuckets) * sizeof(uint32_t);
        uint32_t total = headerSize;
        // For each non-empty bucket, align and add the bytes required for its records.
        for (uint32_t i = 0; i < numBuckets; i++) {
            HashBucket* bucket = hashTable->buckets[i];
            if (bucket != nullptr) {
                total = RoundUp(total, sizeof(uint32_t));
                total += SerialTuple::BytesRequired(bucket);
            }
        }
        return total;
    }

    // Write the HashTable into the provided buffer as a HashBlob.
    // 'bytes' is the total size of the blob (from BytesRequired).
    // Returns a pointer to the filled blob.
    static HashBlob* Write(HashBlob* hb, uint32_t bytes, const Hash* hashTable) {
        uint32_t numBuckets = hashTable->capacity;
        hb->MagicNumber = 0xDEADBEEF;   // Chosen magic number.
        hb->Version = 1;
        hb->BlobSize = bytes;
        hb->NumberOfBuckets = numBuckets;
        // Initialize bucket offsets to 0.
        memset(hb->Buckets, 0, numBuckets * sizeof(uint32_t));

        // The header occupies (4 + numBuckets) uint32_t values.
        uint32_t headerSize = (4 + numBuckets) * sizeof(uint32_t);
        char* writePtr = reinterpret_cast<char*>(hb) + headerSize;

        // Serialize each bucket (if nonempty).
        for (uint32_t i = 0; i < numBuckets; i++) {
            HashBucket* bucket = hashTable->buckets[i];
            if (bucket != nullptr) {
                // Align the write pointer.
                writePtr = reinterpret_cast<char*>(RoundUp(reinterpret_cast<size_t>(writePtr), sizeof(uint32_t)));
                hb->Buckets[i] = writePtr - reinterpret_cast<char*>(hb);
                writePtr = SerialTuple::Write(writePtr, bucket);
            }
        }
        return hb;
    }

    // Create a new HashBlob from the given hash table.
    // Allocates memory, writes the blob, and returns the pointer.
    static HashBlob* Create(const Hash* hashTable) {
        uint32_t totalBytes = BytesRequired(hashTable);
        void* buffer = new char[totalBytes];
        if (!buffer) {
            std::cerr << "Memory allocation failed!" << std::endl;
            return nullptr;
        }
        return Write(reinterpret_cast<HashBlob*>(buffer), totalBytes, hashTable);
    }

    // Discard frees the memory allocated for the HashBlob.
    static void Discard(HashBlob* blob) { delete[] blob; }
};

///////////////////////////////////////////////////////////////////////////////
// HashFile
///////////////////////////////////////////////////////////////////////////////
//
// The HashFile class provides an interface for mapping a file containing a
// HashBlob into memory for fast lookups. It supports both read-only and
// read-write modes.
//
class HashFile {
private:
    HashBlob* blob;
    int fd;
    uint32_t fileSize;

    uint32_t FileSize(int f) {
        struct stat fileInfo;
        fstat(f, &fileInfo);
        return fileInfo.st_size;
    }

public:
    const HashBlob* Blob() { return blob; }

    // Constructor for read-only access.
    HashFile(const char* filename) {
        fd = open(filename, O_RDONLY);
        if (fd < 0) {
            perror("open");
            exit(1);
        }
        fileSize = FileSize(fd);
        blob = reinterpret_cast<HashBlob*>(mmap(nullptr, fileSize, PROT_READ, MAP_SHARED, fd, 0));
        if (blob == MAP_FAILED) {
            perror("mmap");
            exit(1);
        }
        // Optionally, validate header fields (MagicNumber, Version, etc.) here.
    }

    // Constructor for writing a HashBlob.
    // Opens the file for read/write, sets its size, maps it into memory,
    // writes the HashTable as a HashBlob, and synchronizes the data to disk.
    HashFile(const char* filename, const Hash* hashtable) {
        uint32_t bytes = HashBlob::BytesRequired(hashtable);
        fd = open(filename, O_RDWR | O_CREAT | O_TRUNC, 0644);
        if (fd < 0) {
            perror("open");
            exit(1);
        }
        if (ftruncate(fd, bytes) != 0) {
            perror("ftruncate");
            exit(1);
        }
        fileSize = bytes;
        blob = reinterpret_cast<HashBlob*>(mmap(nullptr, fileSize, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0));
        if (blob == MAP_FAILED) {
            perror("mmap");
            exit(1);
        }
        HashBlob::Write(blob, bytes, hashtable);
        // msync ensures that changes to the mapped region are flushed to disk.
        msync(blob, fileSize, MS_SYNC);
    }

    ~HashFile() {
        munmap(blob, fileSize);
        close(fd);
    }
};

///////////////////////////////////////////////////////////////////////////////
// URLBlob
///////////////////////////////////////////////////////////////////////////////
//
// URLBlob is a contiguous serialization of a URLTable.
// Its layout is:
//
// [ Header ]
//   MagicNumber       (uint32_t)
//   Version           (uint32_t)
//   BlobSize          (uint32_t)
//   URLCount          (uint32_t)
//   Offsets[]         (array of uint32_t offsets, one per URL ID)
// [ Serialized Records ]
//   For each URL ID, at its offset:
//
//

class URLBlob {
public:
    uint32_t MagicNumber;        // Magic number for validation
    uint32_t Version;            // Format version
    uint32_t BlobSize;           // Total size of the blob
    uint32_t URLCount;           // Number of URLs
    uint32_t Offsets[Unknown];   // Array of offsets (one per URL ID)

    // Get a URL by its ID
    const char* GetURL(uint32_t urlId) const {
        if (urlId >= URLCount) return "\0";

        uint32_t offset = Offsets[urlId];
        if (offset == 0) return "\0";

        // Skip past the document attributes to get to the URL string
        const char* ptr = reinterpret_cast<const char*>(this) + offset + 5 * sizeof(uint32_t) + sizeof(uint8_t);
        return ptr;
    }

    // Get document attributes by URL ID
    const DocumentAttributes* GetDocumentAttributes(uint32_t urlId) const {
        if (urlId >= URLCount) return nullptr;

        uint32_t offset = Offsets[urlId];
        if (offset == 0) return nullptr;

        DocumentAttributes* attrs = new DocumentAttributes;

        const char* base = reinterpret_cast<const char*>(this) + offset;
        const uint32_t* data = reinterpret_cast<const uint32_t*>(base);

        attrs->wordCount = data[0];
        attrs->urlLength = data[1];
        attrs->titleLength = data[2];
        attrs->startLocation = data[3];
        attrs->endLocation = data[4];

        // TLD is stored immediately after 5 uint32_t's (20 bytes)
        uint8_t packed = *reinterpret_cast<const uint8_t*>(base + 5 * sizeof(uint32_t));
        attrs->english = !!(packed & 0x80);
        attrs->TLD = static_cast<uint8_t>(packed & 0x7F);

        // URL starts right after TLD
        attrs->url = base + 5 * sizeof(uint32_t) + sizeof(uint8_t);

        // Title starts right after the null-terminated URL string
        attrs->title = attrs->url + attrs->urlLength + 1;   // +1 for null terminator
        return attrs;
    }

    // Calculate the total number of bytes required to serialize the URL table
    static uint32_t BytesRequired(const URLTable* table) {
        uint32_t urlCount = table->docAttributes.size();

        // Header: 4 fixed fields + offset array
        uint32_t headerSize = (4 + urlCount) * sizeof(uint32_t);
        uint32_t total = headerSize;

        for (uint32_t i = 0; i < urlCount; i++) {
            const auto& attrs = table->docAttributes[i];

            // Align to uint32_t boundary before each record
            total = RoundUp(total, sizeof(uint32_t));

            // 5 uint32_t fields: wordCount, urlLength, titleLength, startLocation, endLocation
            total += 5 * sizeof(uint32_t);

            // 1 byte for tld
            total += sizeof(uint8_t);

            // URL string (urlLength + 1 for null terminator)
            total += attrs.urlLength + 1;

            // Title string (null terminated, length unknown)
            if (attrs.title) {
                total += strlen(attrs.title) + 1;
            } else {
                total += 1;   // Just the null terminator
            }
        }

        return RoundUp(total, sizeof(uint32_t));
    }


    // Write the URLTable into the provided buffer as a URLBlob
    // 'bytes' is the total size of the blob (from BytesRequired)
    // Returns a pointer to the filled blob
    static URLBlob* Write(URLBlob* blob, uint32_t bytes, const URLTable* table) {
        uint32_t urlCount = table->docAttributes.size();

        blob->MagicNumber = 0xDEADBEEF;
        blob->Version = 1;
        blob->BlobSize = bytes;
        blob->URLCount = urlCount;

        // Zero out offsets
        memset(blob->Offsets, 0, urlCount * sizeof(uint32_t));

        uint32_t headerSize = (4 + urlCount) * sizeof(uint32_t);
        char* writePtr = reinterpret_cast<char*>(blob) + headerSize;

        for (uint32_t i = 0; i < urlCount; i++) {
            const auto& attrs = table->docAttributes[i];

            // Align writePtr
            writePtr = reinterpret_cast<char*>(RoundUp(reinterpret_cast<size_t>(writePtr), sizeof(uint32_t)));

            // Store offset
            blob->Offsets[i] = writePtr - reinterpret_cast<char*>(blob);

            // Write 5 uint32_t fields
            uint32_t* attrPtr = reinterpret_cast<uint32_t*>(writePtr);
            attrPtr[0] = attrs.wordCount;
            attrPtr[1] = attrs.urlLength;
            attrPtr[2] = attrs.titleLength;
            attrPtr[3] = attrs.startLocation;
            attrPtr[4] = attrs.endLocation;
            writePtr += 5 * sizeof(uint32_t);

            // Write tld (1 byte)
            uint8_t tldVal = attrs.TLD;   // 0..127
            uint8_t engFlag = attrs.english ? 0x80 : 0x00;
            *reinterpret_cast<uint8_t*>(writePtr) = engFlag | (tldVal & 0x7F);
            writePtr += sizeof(uint8_t);

            // Write URL string
            memcpy(writePtr, attrs.url, attrs.urlLength);
            writePtr[attrs.urlLength] = '\0';
            writePtr += attrs.urlLength + 1;

            // Write Title string
            if (attrs.title) {
                uint32_t titleLen = strlen(attrs.title);
                memcpy(writePtr, attrs.title, titleLen);
                writePtr[titleLen] = '\0';
                writePtr += titleLen + 1;
            } else {
                *writePtr = '\0';
                writePtr += 1;
            }
        }

        return blob;
    }


    // Create a new URLBlob from the given URLTable
    static URLBlob* Create(const URLTable* table) {
        uint32_t totalBytes = BytesRequired(table);
        void* buffer = new char[totalBytes];
        if (!buffer) {
            std::cerr << "Memory allocation failed!" << std::endl;
            return nullptr;
        }
        return Write(reinterpret_cast<URLBlob*>(buffer), totalBytes, table);
    }

    // Discard frees the memory allocated for the URLBlob
    static void Discard(URLBlob* blob) { delete[] reinterpret_cast<char*>(blob); }
};

///////////////////////////////////////////////////////////////////////////////
// URLFile
///////////////////////////////////////////////////////////////////////////////
//
// The URLFile class provides an interface for mapping a file containing a
// URLBlob into memory for fast lookups.
//
class URLFile {
private:
    URLBlob* blob;
    int fd;
    uint32_t fileSize;

    uint32_t FileSize(int f) {
        struct stat fileInfo;
        fstat(f, &fileInfo);
        return fileInfo.st_size;
    }

public:
    const URLBlob* Blob() { return blob; }

    // Constructor for read-only access
    URLFile(const char* filename) {
        fd = open(filename, O_RDONLY);
        if (fd < 0) {
            perror("open");
            exit(1);
        }
        fileSize = FileSize(fd);
        blob = reinterpret_cast<URLBlob*>(mmap(nullptr, fileSize, PROT_READ, MAP_SHARED, fd, 0));
        if (blob == MAP_FAILED) {
            perror("mmap");
            exit(1);
        }
    }

    // Constructor for writing a URLBlob
    URLFile(const char* filename, const struct URLTable* urlTable) {
        uint32_t bytes = URLBlob::BytesRequired(urlTable);
        fd = open(filename, O_RDWR | O_CREAT | O_TRUNC, 0644);
        if (fd < 0) {
            perror("open");
            exit(1);
        }
        if (ftruncate(fd, bytes) != 0) {
            perror("ftruncate");
            exit(1);
        }
        fileSize = bytes;
        blob = reinterpret_cast<URLBlob*>(mmap(nullptr, fileSize, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0));
        if (blob == MAP_FAILED) {
            perror("mmap");
            exit(1);
        }
        URLBlob::Write(blob, bytes, urlTable);
        // msync ensures that changes to the mapped region are flushed to disk
        msync(blob, fileSize, MS_SYNC);
    }

    // Get a URL by its ID
    const char* GetURL(uint32_t urlId) const { return blob->GetURL(urlId); }

    const DocumentAttributes* GetDocumentAttributes(uint32_t urlId) const { return blob->GetDocumentAttributes(urlId); }

    ~URLFile() {
        munmap(blob, fileSize);
        close(fd);
    }
};
