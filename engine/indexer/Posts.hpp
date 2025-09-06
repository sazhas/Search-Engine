#ifndef POSTS_HPP
#define POSTS_HPP

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

#include "../lib/algorithm.h"
#include "../lib/mutex.h"

// Type definitions
typedef uint32_t FileOffset;
typedef uint32_t Location;

// --------------------------------------------------------------------
// Post Classes
// --------------------------------------------------------------------
class Post {
public:
    virtual Location GetStartLocation() const = 0;
    virtual Location GetEndLocation() const = 0;
    virtual uint32_t GetID() const = 0;
    virtual ~Post() {}
};

class WordPost : public Post {
public:
    Location startLocation;
    uint8_t flags;   // Bit 0: isBold, Bit 1: isHeading, Bit 2: isLargeFont

    WordPost()
        : startLocation(0)
        , flags(0) {}

    WordPost(Location start, uint8_t flags_)
        : startLocation(start)
        , flags(flags_) {}

    Location GetStartLocation() const override { return startLocation; }
    Location GetEndLocation() const override { return startLocation; }
    uint32_t GetID() const override { return 0; }
};

// Helper inline functions for word attributes
inline bool isBold(const WordPost& post) {
    return post.flags & 0x01;
}
inline bool isHeading(const WordPost& post) {
    return post.flags & 0x02;
}
inline bool isLargeFont(const WordPost& post) {
    return post.flags & 0x04;
}
inline void setBold(WordPost& post, bool value) {
    if (value)
        post.flags |= 0x01;
    else
        post.flags &= ~0x01;
}
inline void setHeading(WordPost& post, bool value) {
    if (value)
        post.flags |= 0x02;
    else
        post.flags &= ~0x02;
}
inline void setLargeFont(WordPost& post, bool value) {
    if (value)
        post.flags |= 0x04;
    else
        post.flags &= ~0x04;
}

class DocumentPost : public Post {
public:
    Location startLocation;
    Location endLocation;
    uint32_t docId;

    DocumentPost()
        : startLocation(0)
        , endLocation(0)
        , docId(0) {}

    DocumentPost(Location start, Location end, uint32_t id)
        : startLocation(start)
        , endLocation(end)
        , docId(id) {}

    Location GetStartLocation() const override { return startLocation; }
    Location GetEndLocation() const override { return endLocation; }
    uint32_t GetID() const override { return docId; }
};

// --------------------------------------------------------------------
// Serialization of Posts (Delta Encoding)
// --------------------------------------------------------------------
class SerializedPost {
public:
    // Encode a delta value using a variable-length scheme directly to a buffer.
    // Returns the number of bytes written.
    static uint32_t EncodeVarLengthDelta(uint8_t* buffer, Location delta) {
        uint32_t bytesWritten = 0;
        while (delta >= 0x80) {
            buffer[bytesWritten++] = static_cast<uint8_t>((delta & 0x7F) | 0x80);
            delta >>= 7;
        }
        buffer[bytesWritten++] = static_cast<uint8_t>(delta);
        return bytesWritten;
    }

    // Calculate the number of bytes needed to encode a delta value.
    static uint32_t BytesRequiredForDelta(Location delta) {
        uint32_t bytes = 1;   // At least one byte is needed.
        while (delta >= 0x80) {
            bytes++;
            delta >>= 7;
        }
        return bytes;
    }

    // Decode a delta value from a buffer.
    // Updates bytesRead with the number of bytes consumed.
    static Location DecodeVarLengthDelta(const uint8_t* buffer, uint32_t* bytesRead) {
        Location delta = 0;
        uint32_t shift = 0;
        uint32_t i = 0;
        while (true) {
            uint8_t byte = buffer[i];
            delta |= (byte & 0x7F) << shift;
            i++;
            if (!(byte & 0x80)) break;
            shift += 7;
        }
        *bytesRead = i;
        return delta;
    }

    // Calculate bytes required to serialize a WordPost.
    static uint32_t BytesRequiredForWordPost(const WordPost* post, Location prevLocation) {
        Location delta = post->startLocation - prevLocation;
        uint32_t deltaBytes = BytesRequiredForDelta(delta);
        return deltaBytes + sizeof(post->flags);   // Delta plus one byte for flags.
    }

    // Serialize a WordPost directly to a buffer.
    // Returns the number of bytes written.
    static uint32_t SerializeWordPost(uint8_t* buffer, const WordPost* post, Location prevLocation) {
        Location delta = post->startLocation - prevLocation;
        uint32_t bytesWritten = EncodeVarLengthDelta(buffer, delta);
        buffer[bytesWritten] = post->flags;
        return bytesWritten + 1;   // Delta bytes + one byte for flags.
    }

    static uint8_t* CreateWordPost(const WordPost* post, Location prevLocation) {
        uint32_t bytes = BytesRequiredForWordPost(post, prevLocation);
        uint8_t* buffer = new uint8_t[bytes];
        if (!buffer) {
            std::cerr << "Memory allocation failed!" << std::endl;
            return nullptr;
        }
        SerializeWordPost(buffer, post, prevLocation);
        return buffer;
    }

    // Deserialize a WordPost from a buffer.
    // Updates bytesRead with the number of bytes consumed.
    // Adds the decoded delta to currentLocation.
    static WordPost* DeserializeWordPost(const uint8_t* buffer, uint32_t* bytesRead, Location* currentLocation) {
        uint32_t localBytes = 0;
        Location delta = DecodeVarLengthDelta(buffer, &localBytes);
        *currentLocation += delta;
        WordPost* post = new WordPost();
        post->startLocation = *currentLocation;
        post->flags = buffer[localBytes];   // Read one byte for flags.
        *bytesRead = localBytes + 1;
        return post;
    }

    // Calculate bytes required to serialize a DocumentPost.
    static uint32_t BytesRequiredForDocumentPost(const DocumentPost* post, Location prevEndLocation = 0) {
        uint32_t bytes = 0;
        // Delta between previous document end and this document's start.
        bytes += BytesRequiredForDelta(post->startLocation - prevEndLocation);
        // Document length (end - start).
        bytes += BytesRequiredForDelta(post->endLocation - post->startLocation);
        // Bytes for variable-length encoded docId.
        bytes += BytesRequiredForDelta(post->docId);
        return bytes;
    }

    // Serialize a DocumentPost directly to a buffer.
    // Returns the number of bytes written.
    static uint32_t SerializeDocumentPost(uint8_t* buffer, const DocumentPost* post, Location prevEndLocation = 0) {
        uint32_t bytesWritten = 0;
        // Encode delta from previous document end to this document start.
        bytesWritten += EncodeVarLengthDelta(buffer + bytesWritten, post->startLocation - prevEndLocation);
        // Encode document length (end - start).
        bytesWritten += EncodeVarLengthDelta(buffer + bytesWritten, post->endLocation - post->startLocation);
        // Encode docId using variable-length encoding.
        bytesWritten += EncodeVarLengthDelta(buffer + bytesWritten, post->docId);
        return bytesWritten;
    }

    static uint8_t* CreateDocumentPost(const DocumentPost* post, Location prevLocation) {
        uint32_t bytes = BytesRequiredForDocumentPost(post, prevLocation);
        uint8_t* buffer = new uint8_t[bytes];
        if (!buffer) {
            std::cerr << "Memory allocation failed!" << std::endl;
            return nullptr;
        }
        SerializeDocumentPost(buffer, post, prevLocation);
        return buffer;
    }

    // Deserialize a DocumentPost from a buffer.
    // Returns a newly allocated DocumentPost and sets bytesRead.
    static DocumentPost* DeserializeDocumentPost(const uint8_t* buffer, uint32_t* bytesRead,
                                                 Location& prevEndLocation) {
        uint32_t totalBytes = 0;
        uint32_t localBytes = 0;
        // Decode delta from previous document end.
        Location startDelta = DecodeVarLengthDelta(buffer, &localBytes);
        Location startLoc = prevEndLocation + startDelta;
        totalBytes += localBytes;
        // Decode document length.
        Location docLength = DecodeVarLengthDelta(buffer + totalBytes, &localBytes);
        totalBytes += localBytes;
        // Decode docId.
        uint32_t docId = DecodeVarLengthDelta(buffer + totalBytes, &localBytes);
        totalBytes += localBytes;
        DocumentPost* post = new DocumentPost();
        post->startLocation = startLoc;
        post->endLocation = startLoc + docLength;
        post->docId = docId;
        prevEndLocation = post->endLocation;
        *bytesRead = totalBytes;
        return post;
    }

    static void Discard(uint8_t* blob) { delete[] blob; }
};

// --------------------------------------------------------------------
// Posting List and Its Serialization
// --------------------------------------------------------------------
class PostingList {
public:
    std::vector<uint8_t> rawPostingData;
    uint32_t postCount;
    Location maxLocation;

    PostingList()
        : postCount(0)
        , maxLocation(0) {
        rawPostingData.clear();
    }
    ~PostingList() { /* Nothing to free */
    }

    uint32_t Size() const { return rawPostingData.size(); }

    // Add a word post to the posting list.
    void AddWordPost(const WordPost* post) {
        uint32_t oldSize = rawPostingData.size();
        uint32_t bytesNeeded = SerializedPost::BytesRequiredForWordPost(post, maxLocation);
        rawPostingData.resize(oldSize + bytesNeeded);
        SerializedPost::SerializeWordPost(rawPostingData.data() + oldSize, post, maxLocation);
        postCount++;
        maxLocation = post->startLocation;
    }

    // Add a document post to the posting list.
    void AddDocumentPost(const DocumentPost* post) {
        uint32_t bytesNeeded = SerializedPost::BytesRequiredForDocumentPost(post, maxLocation);
        uint32_t oldSize = rawPostingData.size();
        rawPostingData.resize(oldSize + bytesNeeded);
        SerializedPost::SerializeDocumentPost(rawPostingData.data() + oldSize, post, maxLocation);
        postCount++;
        maxLocation = post->endLocation;
    }

    // Generic add post for backward compatibility.
    void AddPost(const Post* post) {
        const WordPost* wp = dynamic_cast<const WordPost*>(post);
        if (wp)
            AddWordPost(wp);
        else {
            const DocumentPost* dp = dynamic_cast<const DocumentPost*>(post);
            if (dp) AddDocumentPost(dp);
        }
    }

    // Linear scan to find the first WordPost with an absolute location >= target.
    WordPost* SeekWordPost(Location target) {
        uint32_t offset = 0;
        Location currentLocation = 0;
        while (offset < rawPostingData.size()) {
            uint32_t bytesRead = 0;
            WordPost* post
              = SerializedPost::DeserializeWordPost(rawPostingData.data() + offset, &bytesRead, &currentLocation);
            if (!post) break;
            if (currentLocation >= target) return post;
            offset += bytesRead;
            delete post;
        }
        return nullptr;
    }

    // Linear scan to find the first DocumentPost with an absolute location >= target.
    DocumentPost* SeekDocumentPost(Location target) {
        uint32_t offset = 0;
        Location prevEndLocation = 0;
        while (offset < rawPostingData.size()) {
            uint32_t bytesRead = 0;
            DocumentPost* post
              = SerializedPost::DeserializeDocumentPost(rawPostingData.data() + offset, &bytesRead, prevEndLocation);
            if (!post) break;
            if (post->endLocation >= target) return post;
            prevEndLocation = post->endLocation;
            offset += bytesRead;
            delete post;
        }
        return nullptr;
    }

    uint32_t GetPostCount() const { return postCount; }
};

// --------------------------------------------------------------------
// SerializedPostingList
// --------------------------------------------------------------------
// Represents a posting list stored as one contiguous buffer.
// Layout:
// [ bytes (uint32_t) ]
// [ skipCount (uint32_t) ]
// [ postCount (uint32_t) ]
// [ SkipEntry[0] ]
// [ SkipEntry[1] ]
//    ...
// [ Serialized Posting Data ]
class SerializedPostingList {
public:
    uint32_t bytes;             // Total size in bytes (header + data + any padding)
    uint32_t postingDataSize;   // Actual size of rawPostingData (without padding)
    uint32_t skipCount;         // Number of skip entries (computed dynamically)
    uint32_t postCount;         // Number of posts in the list

    // Skip table entry structure.
    struct SkipEntry {
        FileOffset Offset;       // Offset (in bytes) relative to the posting data.
        Location PostLocation;   // Absolute post location at that offset.
    };

    // Helper to compute the number of skip entries.
    static inline uint32_t ComputeSkipCount(uint32_t numPosts, uint32_t postsPerSkip = 32, uint32_t maxSkips = 256) {
        uint32_t computed = (numPosts >= postsPerSkip) ? numPosts / postsPerSkip : 1;
        return my_min(computed, maxSkips);
    }

    // Dynamic bucket index calculation using the dynamic skip table size.
    static inline uint8_t GetBucketIndex(Location loc, Location maxLoc, uint32_t skipTableSize) {
        if (maxLoc == 0) return 0;
        if (loc > maxLoc) return static_cast<uint8_t>(skipTableSize - 1);
        return static_cast<uint8_t>((loc * skipTableSize) / (maxLoc + 1));
    }

    // Returns a pointer to the skip table array.
    const SkipEntry* GetSkipTable() const {
        return reinterpret_cast<const SkipEntry*>(reinterpret_cast<const uint8_t*>(this) + 4 * sizeof(uint32_t));
    }

    // Returns a pointer to the posting data (immediately after the skip table).
    const uint8_t* GetPostingData() const {
        return reinterpret_cast<const uint8_t*>(this) + 4 * sizeof(uint32_t) + skipCount * sizeof(SkipEntry);
    }

    WordPost* GetCurrentWord(const uint8_t** ptr, Location& currentLocation) const {
        const uint8_t* list = GetPostingData();
        if (*ptr >= list && *ptr < list + postingDataSize) {
            uint32_t bytesRead;
            WordPost* wp = SerializedPost::DeserializeWordPost(*ptr, &bytesRead, &currentLocation);
            *ptr += bytesRead;
            return wp;
        }
        return nullptr;
    }

    DocumentPost* GetCurrentDoc(const uint8_t** ptr, Location& currentLocation) const {
        const uint8_t* list = GetPostingData();
        if (*ptr >= list && *ptr < list + postingDataSize) {
            uint32_t bytesRead;
            DocumentPost* dp = SerializedPost::DeserializeDocumentPost(*ptr, &bytesRead, currentLocation);
            *ptr += bytesRead;
            return dp;
        }
        return nullptr;
    }

    uint32_t GetPostingDataSize() const { return postingDataSize; }

    // Find the best skip entry for a given target location.
    const SkipEntry* FindBestSkipEntry(const SkipEntry* table, Location target, Location currentLocation,
                                       Location maxLocation) const {
        uint8_t targetBucket = GetBucketIndex(target, maxLocation, skipCount);
        if (target <= currentLocation) return nullptr;
        if (targetBucket < skipCount) {
            const SkipEntry& entry = table[targetBucket];
            if (entry.PostLocation > currentLocation && entry.PostLocation < target) return &entry;
        }
        return nullptr;
    }

    // Seek for a WordPost given a target location, updating the pointer and current location.
    WordPost* SeekWordPost(Location target, Location& currentLocation, const uint8_t*& data) const {
        uint32_t offset = 0;
        if (currentLocation >= target) {
            currentLocation = 0;
            data = GetPostingData();
        }
        const SkipEntry* table = GetSkipTable();
        Location maxLocation = (skipCount > 0) ? table[skipCount - 1].PostLocation : 0;
        // Try to use the skip table.
        const SkipEntry* bestEntry = FindBestSkipEntry(table, target, currentLocation, maxLocation);
        if (bestEntry) {
            offset = bestEntry->Offset;
            currentLocation = bestEntry->PostLocation;
            data = GetPostingData() + offset;
        }
        // Linear scan from that position.
        while (data < GetPostingData() + postingDataSize) {
            uint32_t bytesRead = 0;
            WordPost* post = SerializedPost::DeserializeWordPost(data, &bytesRead, &currentLocation);
            if (!post) break;
            data += bytesRead;
            if (currentLocation >= target) {
                return post;
            }
            delete post;
        }
        return nullptr;
    }

    // Seek for a DocumentPost given a target location, updating the pointer and current location.
    DocumentPost* SeekDocumentPost(Location target, Location& prevEndLocation, const uint8_t*& data) const {
        uint32_t offset = 0;
        if (prevEndLocation >= target) {
            prevEndLocation = 0;
            data = GetPostingData();
        }
        const SkipEntry* table = GetSkipTable();
        Location maxLocation = (skipCount > 0) ? table[skipCount - 1].PostLocation : 0;
        const SkipEntry* bestEntry = FindBestSkipEntry(table, target, prevEndLocation, maxLocation);
        if (bestEntry) {
            offset = bestEntry->Offset;
            prevEndLocation = bestEntry->PostLocation;
            data = GetPostingData() + offset;
        }
        while (data < GetPostingData() + postingDataSize) {
            uint32_t bytesRead = 0;
            DocumentPost* post = SerializedPost::DeserializeDocumentPost(data, &bytesRead, prevEndLocation);
            if (!post) break;
            data += bytesRead;
            if (post->endLocation >= target) {
                return post;
            }
            delete post;
        }
        return nullptr;
    }

    // Backward-compatible seek functions.
    WordPost* SeekWordPost(Location target) const {
        const uint8_t* tempData = GetPostingData();
        Location currentLocation = 0;
        return SeekWordPost(target, currentLocation, tempData);
    }
    DocumentPost* SeekDocumentPost(Location target) const {
        const uint8_t* tempData = GetPostingData();
        Location prevEndLocation = 0;
        return SeekDocumentPost(target, prevEndLocation, tempData);
    }

    // Build a dynamic skip table for WordPosts.
    static std::vector<SkipEntry> BuildWordPostSkipTable(const std::vector<uint8_t>& rawData, Location maxLocation,
                                                         uint32_t numPosts) {
        uint32_t dynamicSkipCount = ComputeSkipCount(numPosts, 32, 256);
        std::vector<SkipEntry> skipEntries(dynamicSkipCount, { 0, 0 });
        uint32_t offsetInRaw = 0;
        Location currentLocation = 0;
        uint32_t lastBucket = 0;
        skipEntries[0].Offset = static_cast<FileOffset>(offsetInRaw);
        skipEntries[0].PostLocation = 0;
        while (offsetInRaw < rawData.size() && dynamicSkipCount > 0) {
            uint32_t bytesRead = 0;
            Location oldLocation = currentLocation;
            WordPost* post
              = SerializedPost::DeserializeWordPost(rawData.data() + offsetInRaw, &bytesRead, &currentLocation);
            if (!post) break;
            uint32_t bucket = GetBucketIndex(currentLocation, maxLocation, dynamicSkipCount);
            if (bucket > lastBucket && bucket < dynamicSkipCount) {
                for (uint32_t b = lastBucket + 1; b <= bucket && b < dynamicSkipCount; b++) {
                    skipEntries[b].Offset = static_cast<FileOffset>(offsetInRaw);
                    skipEntries[b].PostLocation = oldLocation;
                }
                lastBucket = bucket;
            }
            offsetInRaw += bytesRead;
            delete post;
        }
        for (uint32_t b = lastBucket + 1; b < dynamicSkipCount && b > lastBucket; b++) {
            skipEntries[b].Offset = static_cast<FileOffset>(offsetInRaw);
            skipEntries[b].PostLocation = currentLocation;
        }
        return skipEntries;
    }

    // Build a dynamic skip table for DocumentPosts.
    static std::vector<SkipEntry> BuildDocumentPostSkipTable(const std::vector<uint8_t>& rawData, Location maxLocation,
                                                             uint32_t numPosts) {
        uint32_t dynamicSkipCount = ComputeSkipCount(numPosts, 32, 256);
        std::vector<SkipEntry> skipEntries(dynamicSkipCount, { 0, 0 });
        uint32_t offsetInRaw = 0;
        Location prevEndLocation = 0;
        uint32_t lastBucket = 0;
        skipEntries[0].Offset = static_cast<FileOffset>(offsetInRaw);
        skipEntries[0].PostLocation = 0;
        while (offsetInRaw < rawData.size() && dynamicSkipCount > 0) {
            uint32_t bytesRead = 0;
            Location temp = prevEndLocation;
            DocumentPost* post
              = SerializedPost::DeserializeDocumentPost(rawData.data() + offsetInRaw, &bytesRead, prevEndLocation);
            if (!post) break;
            uint32_t bucket = GetBucketIndex(post->endLocation, maxLocation, dynamicSkipCount);
            if (bucket > lastBucket && bucket < dynamicSkipCount) {
                for (uint32_t b = lastBucket + 1; b <= bucket && b < dynamicSkipCount; b++) {
                    skipEntries[b].Offset = static_cast<FileOffset>(offsetInRaw);
                    skipEntries[b].PostLocation = temp;
                }
                lastBucket = bucket;
            }
            prevEndLocation = post->endLocation;
            offsetInRaw += bytesRead;
            delete post;
        }
        for (uint32_t b = lastBucket + 1; b < dynamicSkipCount && b > lastBucket; b++) {
            skipEntries[b].Offset = static_cast<FileOffset>(offsetInRaw);
            skipEntries[b].PostLocation = prevEndLocation;
        }
        return skipEntries;
    }

    // Write the WordPostingList into a pre-allocated buffer.
    static SerializedPostingList* WriteWordPostingList(uint8_t* out, const PostingList& plist) {
        SerializedPostingList* result = reinterpret_cast<SerializedPostingList*>(out);
        // First pass: find max location.
        Location maxLocation = 0;
        uint32_t offset = 0;
        Location currentLocation = 0;
        while (offset < plist.rawPostingData.size()) {
            uint32_t bytesRead = 0;
            WordPost* post
              = SerializedPost::DeserializeWordPost(plist.rawPostingData.data() + offset, &bytesRead, &currentLocation);
            if (!post) break;
            maxLocation = currentLocation;
            offset += bytesRead;
            delete post;
        }
        uint32_t numPosts = plist.GetPostCount();
        uint32_t dynamicSkipCount = ComputeSkipCount(numPosts, 32, 256);
        uint32_t totalBytes = 4 * sizeof(uint32_t) + dynamicSkipCount * sizeof(SkipEntry) + plist.rawPostingData.size();
        totalBytes = RoundUp(totalBytes, sizeof(uint32_t));

        result->bytes = totalBytes;
        result->postingDataSize = plist.rawPostingData.size();
        result->skipCount = dynamicSkipCount;
        result->postCount = numPosts;

        // Move pointer past header.
        uint8_t* skipTableOut = out + 4 * sizeof(uint32_t);
        std::vector<SkipEntry> skipEntries = BuildWordPostSkipTable(plist.rawPostingData, maxLocation, numPosts);
        memcpy(skipTableOut, skipEntries.data(), dynamicSkipCount * sizeof(SkipEntry));

        uint8_t* dataOut = skipTableOut + dynamicSkipCount * sizeof(SkipEntry);
        memcpy(dataOut, plist.rawPostingData.data(), plist.rawPostingData.size());

        return result;
    }

    // Write the DocumentPostingList into a pre-allocated buffer.
    static SerializedPostingList* WriteDocumentPostingList(uint8_t* out, const PostingList& plist) {
        SerializedPostingList* result = reinterpret_cast<SerializedPostingList*>(out);
        // First pass: find max location.
        Location maxLocation = 0;
        uint32_t offset = 0;
        Location prevEndLocation = 0;
        while (offset < plist.rawPostingData.size()) {
            uint32_t bytesRead = 0;
            DocumentPost* post = SerializedPost::DeserializeDocumentPost(plist.rawPostingData.data() + offset,
                                                                         &bytesRead, prevEndLocation);
            if (!post) break;
            maxLocation = std::max(maxLocation, post->endLocation);
            prevEndLocation = post->endLocation;
            offset += bytesRead;
            delete post;
        }
        uint32_t numPosts = plist.GetPostCount();
        uint32_t dynamicSkipCount = ComputeSkipCount(numPosts, 32, 256);
        uint32_t totalBytes = 4 * sizeof(uint32_t) + dynamicSkipCount * sizeof(SkipEntry) + plist.rawPostingData.size();
        totalBytes = RoundUp(totalBytes, sizeof(uint32_t));

        result->bytes = totalBytes;
        result->postingDataSize = plist.rawPostingData.size();
        result->skipCount = dynamicSkipCount;
        result->postCount = numPosts;

        uint8_t* skipTableOut = out + 4 * sizeof(uint32_t);
        std::vector<SkipEntry> skipEntries = BuildDocumentPostSkipTable(plist.rawPostingData, maxLocation, numPosts);
        memcpy(skipTableOut, skipEntries.data(), dynamicSkipCount * sizeof(SkipEntry));

        uint8_t* dataOut = skipTableOut + dynamicSkipCount * sizeof(SkipEntry);
        memcpy(dataOut, plist.rawPostingData.data(), plist.rawPostingData.size());

        return result;
    }

    // Calculate bytes required to serialize a posting list with a dynamic skip table.
    static uint32_t BytesRequired(const PostingList& plist) {
        uint32_t numPosts = plist.GetPostCount();
        uint32_t dynamicSkipCount = ComputeSkipCount(numPosts, 32, 256);
        uint32_t headerSize = 4 * sizeof(uint32_t) + dynamicSkipCount * sizeof(SkipEntry);
        uint32_t dataSize = plist.rawPostingData.size();
        return RoundUp(headerSize + dataSize, sizeof(uint32_t));
    }

    // Create a new WordPostingList with a dynamic skip table.
    static SerializedPostingList* CreateWordPostingList(const PostingList& plist) {
        uint32_t bytes = BytesRequired(plist);
        uint8_t* buffer = new uint8_t[bytes];
        if (!buffer) {
            std::cerr << "Memory allocation failed!" << std::endl;
            return nullptr;
        }
        return WriteWordPostingList(buffer, plist);
    }

    // Create a new DocumentPostingList with a dynamic skip table.
    static SerializedPostingList* CreateDocumentPostingList(const PostingList& plist) {
        uint32_t bytes = BytesRequired(plist);
        uint8_t* buffer = new uint8_t[bytes];
        if (!buffer) {
            std::cerr << "Memory allocation failed!" << std::endl;
            return nullptr;
        }
        return WriteDocumentPostingList(buffer, plist);
    }

    // Free the memory allocated for a SerializedPostingList.
    static void Discard(SerializedPostingList* buffer) { delete[] reinterpret_cast<uint8_t*>(buffer); }
};

#endif   // POSTS_HPP