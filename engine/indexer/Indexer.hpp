#ifndef INDEXER_HPP
#define INDEXER_HPP

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <unordered_set>
#include <vector>

#include "../lib/HashTable.h"
#include "../lib/file.h"
#include "../lib/mutex.h"
#include "../lib/stemmer.h"
#include "../parser/HtmlParser.h"
#include "HashBlob.h"
#include "Posts.hpp"

const size_t UNIQUE_WORDS_THRESHOLD = 20;

class ISR;
class ISRWord;
struct Index;
class IndexBlob;

class ISR {
public:
    ISR()
        : currLocation(0)
        , current(nullptr) {}
    virtual uint32_t GetPostCount() = 0;
    virtual Post* GetCurrentDoc() = 0;
    virtual Post* Next() = 0;
    virtual Post* NextInternal() = 0;
    virtual Post* Seek(Location target) = 0;
    virtual Location GetStartLocation() = 0;
    virtual Location GetEndLocation() = 0;
    virtual Post* GetCurrentPost() = 0;

    virtual bool isSynonym() { return false; }
    virtual bool isSynonymWord() { return syn_word; }
    virtual void setSynonym(bool syn) { syn_word = syn; }
    virtual ~ISR() {}
    virtual void collectTerms(IndexBlob* index, std::vector<ISRWord*>& terms,
                              std::unordered_set<std::string>& terms_set) {}

protected:
    Location currLocation;
    Post* current;
    bool syn_word = false;
};

// Document-level iterator
class ISRDoc : public ISR {
public:
    ISRDoc(const URLBlob* docTable_, const SerializedPostingList* plist_, const uint8_t* data_)
        : docTable(docTable_)
        , plist(plist_)
        , data(data_) {}

    uint32_t GetPostCount() override { return plist->postCount; }

    Post* NextInternal() override { return Next(); }

    Post* Next() override {
        if (current) {
            delete current;
            current = nullptr;
        }
        current = plist->GetCurrentDoc(&data, currLocation);
        return current;
    }

    Post* Seek(Location target) override {
        if (current && GetEndLocation() >= target) {
            return current;
        }
        if (current) {
            delete current;
            current = nullptr;
        }
        current = plist->SeekDocumentPost(target, currLocation, data);
        return current;
    }

    Location GetStartLocation() override {
        if (current) return current->GetStartLocation();
        return 0;
    }

    Location GetEndLocation() override {
        if (current) return current->GetEndLocation();
        return 0;
    }

    Post* GetCurrentPost() override { return current; }

    void collectTerms(IndexBlob*, std::vector<ISRWord*>&, std::unordered_set<std::string>& terms_set) override {}

    unsigned GetDocumentLength() {
        if (current) {
            return current->GetEndLocation() - current->GetStartLocation();
        }
        return 0;
    }

    unsigned GetWordCount() {
        if (current) {
            uint32_t docID = current->GetID();
            auto attributes = docTable->GetDocumentAttributes(docID);
            return attributes->wordCount;
        }
        return 0;
    }

    unsigned GetUrlLength() {
        if (current) {
            uint32_t docID = current->GetID();
            auto attributes = docTable->GetDocumentAttributes(docID);
            return attributes->urlLength;
        }
        return 0;
    }

    uint8_t GetTLD() {
        if (current) {
            uint32_t docID = current->GetID();
            auto attributes = docTable->GetDocumentAttributes(docID);
            return attributes->TLD;
        }
        return 0;
    }

    Post* GetCurrentDoc() override { return GetCurrentPost(); }

    const char* GetURL() {
        if (current) {
            uint32_t docID = current->GetID();
            return docTable->GetURL(docID);
        }
        return "\0";
    }

    const DocumentAttributes* GetDocumentAttributes() {
        if (current) {
            uint32_t docID = current->GetID();
            return docTable->GetDocumentAttributes(docID);
        }
        return nullptr;
    }

    ~ISRDoc() {
        if (current) {
            delete current;
        }
    }

private:
    const URLBlob* docTable;
    const SerializedPostingList* plist;
    const uint8_t* data;
};

// Word-level iterator
class ISRWord : public ISR {
public:
    ISRWord(const char* word, const SerializedPostingList* plist_, const uint8_t* data_, ISRDoc* isrdoc)
        : plist(plist_)
        , data(data_)
        , isr_doc(isrdoc)
        , key(strdup(word)) {}

    const char* GetKey() { return key; }

    Post* GetCurrentDoc() override {
        if (current) {
            return isr_doc->Seek(current->GetStartLocation());
        }
        return nullptr;
    }

    uint32_t GetPostCount() override { return plist->postCount; }

    Post* NextInternal() override {
        if (current) {
            delete current;
            current = nullptr;
        }
        current = plist->GetCurrentWord(&data, currLocation);
        return current;
    }

    Post* Next() override {
        Location target = 0;
        if (current) {
            auto post = isr_doc->Seek(current->GetStartLocation());
            if (post) {
                target = post->GetEndLocation() + 1;
            }
        }
        return Seek(target);
    }

    Post* Seek(Location target) override {
        if (current && GetStartLocation() >= target) {
            return current;
        }
        if (current) {
            delete current;
            current = nullptr;
        }
        current = plist->SeekWordPost(target, currLocation, data);
        return current;
    }

    Location GetStartLocation() override {
        if (current) return current->GetStartLocation();
        return 0;
    }

    Location GetEndLocation() override {
        if (current) return current->GetEndLocation();
        return 0;
    }

    Post* GetCurrentPost() override { return current; }

    unsigned GetDocumentCount() {
        // Save current state
        const uint8_t* savedData = data;
        Location savedLocation = currLocation;
        Post* savedCurrent = current;
        current = nullptr;   // Don't delete the saved current

        unsigned count = 0;
        if (savedCurrent) {
            count = 1;
        }

        // Get first document
        while (Next()) {
            // For each document, seek to its start location
            count++;
        }

        // Restore state
        data = savedData;
        currLocation = savedLocation;
        if (current) delete current;
        current = savedCurrent;

        return count;
    }

    unsigned GetOccurrencesInCurrDoc(Location startLocation, Location endLocation) {
        if (current && GetStartLocation() > endLocation) {
            return 0;
        }

        // Save current state
        const uint8_t* savedData = data;
        Location savedLocation = currLocation;
        Post* savedCurrent = current;
        current = nullptr;   // Don't delete the saved current

        unsigned count = 0;

        // Seek to start of document
        Post* word = Seek(startLocation);

        // Count occurrences within document bounds
        while (word && word->GetStartLocation() <= endLocation) {
            count++;
            word = NextInternal();
        }

        // Restore state
        data = savedData;
        currLocation = savedLocation;
        if (current) delete current;
        current = savedCurrent;

        return count;
    }

    // void collectTerms(IndexBlob * index, std::vector<ISRWord*>& terms) { terms.push_back(index->OpenISRWord(key)); }
    void collectTerms(IndexBlob* index, std::vector<ISRWord*>& terms, std::unordered_set<std::string>& terms_set);

    ~ISRWord() {
        if (key) {
            free((void*) key);
        }

        if (current) {
            delete current;
        }

        delete isr_doc;
    }

private:
    const SerializedPostingList* plist;
    const uint8_t* data;
    const char* key;
    ISRDoc* isr_doc;
};

class ISRAbstract : public ISRWord {
public:
    // Call the ISRWord constructor with (nullptr) arguments.
    ISRAbstract()
        : ISRWord("", nullptr, nullptr, nullptr) {}

    // Override the abstract methods to return 0 or nullptr.
    virtual uint32_t GetPostCount() override { return 0; }
    virtual Post* GetCurrentDoc() override { return nullptr; }
    virtual Post* Next() override { return nullptr; }
    virtual Post* NextInternal() override { return nullptr; }
    virtual Post* Seek(Location /*target*/) override { return nullptr; }
    virtual Location GetStartLocation() override { return 0; }
    virtual Location GetEndLocation() override { return 0; }
    virtual Post* GetCurrentPost() override { return nullptr; }
    void collectTerms(IndexBlob*, std::vector<ISRWord*>&, std::unordered_set<std::string>&) override {}
    virtual ~ISRAbstract() override {}
};

// --------------------------------------------------------------------
// Index
// --------------------------------------------------------------------
struct Index {
    Location WordsInIndex = 0;
    Location DocumentsInIndex = 0;
    Location LocationsInIndex = 0;
    Location MaximumLocation = 0;

    URLTable urlTable;
    HashTable<const char*, PostingList*> dictionary;
    PostingList* docEnd;
    Mutex indexMutex;   // Single mutex guarding the entire index

    Index() { docEnd = new PostingList; }

    ~Index() {
        for (auto it = dictionary.begin(); it != dictionary.end(); ++it) {
            free((void*) it->key);
            delete it->value;
        }
        delete docEnd;
    }

    void AddAnchor(Link& link) {
        // Placeholder for future anchor indexing
    }

    void AddTitle(string& token, Location& nextLocation) {
        token = "@" + token;
        WordPost post = { nextLocation++, 0 };

        PostingList* list = nullptr;

        auto entry = dictionary.Find(token.c_str());
        if (!entry) {
            list = new PostingList;
            char* keyCopy = strdup(token.c_str());
            entry = dictionary.Find(keyCopy, list);
            if (!entry) {
                delete list;
                free(keyCopy);
                return;
            }
            WordsInIndex++;
        } else {
            list = entry->value;
        }

        list->AddWordPost(&post);
        LocationsInIndex++;
    }

    void AddWord(string& token, uint8_t& flags, Location& nextLocation) {
        WordPost post = { nextLocation++, flags };
        PostingList* list = nullptr;

        auto entry = dictionary.Find(token.c_str());
        if (!entry) {
            char* keyCopy = strdup(token.c_str());
            list = new PostingList;
            entry = dictionary.Find(keyCopy, list);
            if (!entry) {
                delete list;
                free(keyCopy);
                return;
            }
            WordsInIndex++;
        } else {
            list = entry->value;
        }

        list->AddWordPost(&post);
        LocationsInIndex++;
    }

    void Insert(HtmlParser* parsedURL) {
        if (parsedURL->titleWords.size() >= 40) {
            return;
        }
        size_t totalLocationsNeeded = parsedURL->titleWords.size() + parsedURL->words_flags.size() + 2;

        char* keyCopyURL = strdup(parsedURL->pageURL.c_str());
        char* titleCopy = strdup(parsedURL->title_chunk.c_str());

        indexMutex.lock();

        Location startLocation = MaximumLocation + 1;
        MaximumLocation += totalLocationsNeeded;
        Location endLocation = startLocation + totalLocationsNeeded - 1;

        uint32_t id = urlTable.AddURL(keyCopyURL);
        urlTable.SetDocumentAttributes(titleCopy, id, parsedURL->words_flags.size() + parsedURL->titleWords.size(),
                                       strlen(keyCopyURL), parsedURL->titleWords.size(), startLocation, endLocation,
                                       parsedURL->english);

        DocumentPost post = { startLocation, endLocation, id };
        docEnd->AddDocumentPost(&post);
        DocumentsInIndex++;
        LocationsInIndex++;

        Location nextLocation = startLocation;
        for (auto& token : parsedURL->titleWords) {
            auto stem = Stemmer::stem(token);
            if (!stem.empty()) {
                AddTitle(stem, nextLocation);
            }
        }
        for (auto& token : parsedURL->words_flags) {
            auto stem = Stemmer::stem(token.word);
            if (!stem.empty()) {
                AddWord(stem, token.flags, nextLocation);
            }
        }
        for (auto& token : parsedURL->links) {
            AddAnchor(token);
        }
        indexMutex.unlock();
    }
};

// --------------------------------------------------------------------
// IndexBlob and IndexFile Interfaces
// --------------------------------------------------------------------
class IndexBlob {
public:
    Location WordsInIndex;
    Location DocumentsInIndex;
    Location LocationsInIndex;
    Location MaximumLocation;
    uint32_t sizeOfURLs;
    uint32_t sizeOfHash;

    // Write array of URL C-strings and the hash blob for the dictionary below this header.
    const SerializedPostingList* Find(const char* key) const {
        const HashBlob* blob = GetHashBlob();
        auto entry = blob->Find(key);
        if (entry) {
            // Convert the offset to a pointer by adding it to the entry's address
            return reinterpret_cast<const SerializedPostingList*>(reinterpret_cast<const char*>(entry) + entry->Value);
        }
        return nullptr;
    }

    const SerializedPostingList* GetDocEnd() const {
        const char* ptr = reinterpret_cast<const char*>(this) + (6 * sizeof(uint32_t)) + sizeOfURLs + sizeOfHash;
        return reinterpret_cast<const SerializedPostingList*>(ptr);
    }

    const char* GetURL(uint32_t urlId) const {
        const URLBlob* blob = GetURLTable();
        return blob->GetURL(urlId);
    }

    const DocumentAttributes* GetDocAttributes(uint32_t urlId) const {
        const URLBlob* blob = GetURLTable();
        return blob->GetDocumentAttributes(urlId);
    }

    const URLBlob* GetURLTable() const {
        const char* ptr = reinterpret_cast<const char*>(this) + (6 * sizeof(uint32_t));
        return reinterpret_cast<const URLBlob*>(ptr);
    }

    const HashBlob* GetHashBlob() const {
        const char* ptr = reinterpret_cast<const char*>(this) + (6 * sizeof(uint32_t)) + sizeOfURLs;
        return reinterpret_cast<const HashBlob*>(ptr);
    }

    static IndexBlob* Write(IndexBlob* hb, uint32_t urlBytes, uint32_t hashBytes, const Index* index) {
        hb->WordsInIndex = index->WordsInIndex;
        hb->DocumentsInIndex = index->DocumentsInIndex;
        hb->LocationsInIndex = index->LocationsInIndex;
        hb->MaximumLocation = index->MaximumLocation;
        hb->sizeOfURLs = urlBytes;
        hb->sizeOfHash = hashBytes;

        uint32_t headerSize = 6 * sizeof(uint32_t);
        char* writePtr = reinterpret_cast<char*>(hb) + headerSize;
        URLBlob::Write(reinterpret_cast<URLBlob*>(writePtr), urlBytes, &index->urlTable);

        writePtr += hb->sizeOfURLs;
        HashBlob::Write(reinterpret_cast<HashBlob*>(writePtr), hashBytes, &index->dictionary);

        writePtr += hb->sizeOfHash;
        SerializedPostingList::WriteDocumentPostingList(reinterpret_cast<uint8_t*>(writePtr), *index->docEnd);
        return hb;
    }

    static uint32_t BytesRequired(const Index* index) {
        uint32_t urlBytes = URLBlob::BytesRequired(&index->urlTable);
        uint32_t totalBytes = urlBytes + (6 * sizeof(uint32_t)) + HashBlob::BytesRequired(&index->dictionary)
                            + SerializedPostingList::BytesRequired(*index->docEnd);
        return totalBytes;
    }

    // Create a new IndexBlob from the given index.
    static IndexBlob* Create(const Index* index) {
        uint32_t urlBytes = URLBlob::BytesRequired(&index->urlTable);
        uint32_t hashBytes = HashBlob::BytesRequired(&index->dictionary);
        uint32_t totalBytes
          = urlBytes + (6 * sizeof(uint32_t)) + hashBytes + SerializedPostingList::BytesRequired(*index->docEnd);
        void* buffer = new char[totalBytes];
        if (!buffer) {
            std::cerr << "Memory allocation failed!" << std::endl;
            return nullptr;
        }
        return Write(reinterpret_cast<IndexBlob*>(buffer), urlBytes, hashBytes, index);
    }

    // Free the memory allocated for the IndexBlob.
    static void Discard(IndexBlob* blob) { delete[] blob; }

    ISRWord* OpenISRWord(const char* word) {
        auto list = Find(word);
        if (list) {
            auto data = list->GetPostingData();
            return new ISRWord(word, list, data, OpenISREndDoc());
        }
        return new ISRAbstract();
    }

    ISRDoc* OpenISREndDoc() {
        auto list = GetDocEnd();
        if (list) {
            auto data = list->GetPostingData();
            auto docTable = GetURLTable();
            return new ISRDoc(docTable, list, data);
        }
        return nullptr;
    }
};

class IndexFile {
private:
    int fd;
    size_t fileSize;
    bool closed;

public:
    size_t Size() const { return fileSize; }
    IndexBlob* blob;
    IndexFile(const char* filename)
        : closed(false) {
        fd = open(filename, O_RDONLY);
        if (fd < 0) {
            perror("open");
            exit(1);
        }
        fileSize = file_size(fd);
        blob = reinterpret_cast<IndexBlob*>(mmap(nullptr, fileSize, PROT_READ, MAP_SHARED, fd, 0));
        if (blob == MAP_FAILED) {
            perror("mmap");
            exit(1);
        }
    }
    IndexFile(const char* filename, const Index* index)
        : closed(false) {
        uint32_t urlBytes = URLBlob::BytesRequired(&index->urlTable);
        uint32_t hashBytes = HashBlob::BytesRequired(&index->dictionary);
        uint32_t bytes
          = urlBytes + (6 * sizeof(uint32_t)) + hashBytes + SerializedPostingList::BytesRequired(*index->docEnd);
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
        blob = reinterpret_cast<IndexBlob*>(mmap(nullptr, fileSize, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0));
        if (blob == MAP_FAILED) {
            perror("mmap");
            exit(1);
        }
        IndexBlob::Write(blob, urlBytes, hashBytes, index);
        msync(blob, fileSize, MS_SYNC);
    }

    void close_file() {
        if (!closed) {
            munmap(blob, fileSize);
            close(fd);
            blob = nullptr;
            fd = -1;
            closed = true;
        }
    }

    ~IndexFile() { close_file(); }
};

inline void ISRWord::collectTerms(IndexBlob* index, std::vector<ISRWord*>& terms,
                                  std::unordered_set<std::string>& terms_set) {
    std::string keyCopy(key);
    if (terms_set.find(key) == terms_set.end()) {
        terms_set.insert(keyCopy);
        terms.push_back(index->OpenISRWord(key));
    }
}

#endif   // INDEXER_HPP
