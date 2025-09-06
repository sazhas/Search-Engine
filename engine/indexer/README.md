# Indexer Interface Documentation

This document describes how to use the indexer interface for inserting documents, creating serialized blobs, and searching through the index.

## Table of Contents
- [Basic Usage](#basic-usage)
- [Inserting Documents](#inserting-documents)
- [Creating Serialized Index](#creating-serialized-index)
- [Searching and Iterating](#searching-and-iterating)
- [Statistics and Metadata](#statistics-and-metadata)

## Basic Usage

```cpp
#include "indexer/Indexer.hpp"
#include "parser/HtmlParser.h"

// Create a new index
Index index;
```

## Inserting Documents

The indexer supports inserting parsed HTML documents with their associated URLs:

```cpp
// Parse an HTML document
HtmlParser parser;
parser.Parse(htmlContent);

// Insert the document into the index
// This will:
// - Add title words (prefixed with @)
// - Add regular words
// - Add anchor text (links)
// - Store document metadata
index.Insert(&parser, "http://example.com/page");
```

## Creating Serialized Index

The index can be serialized in two ways:

### 1. In-Memory Blob

```cpp
// Create an in-memory serialized blob
IndexBlob* blob = IndexBlob::Create(&index);

// Use the blob...

// Clean up when done
IndexBlob::Discard(blob);
```

### 2. Memory-Mapped File

```cpp
// Writing to a new file
IndexFile file("index.bin", &index);

// Loading an existing file (read-only)
IndexFile existingFile("index.bin");  // Opens in read-only mode

// The file will be automatically unmapped in the destructor

//Access blob from file like so
file.blob->Find("word");
```

## Searching and Iterating

### Finding Posting Lists

```cpp
// Using IndexBlob or IndexFile
const SerializedPostingList* postings = blob->Find("word");
if (postings != nullptr) {
    // Access posting list data
    size_t size = postings->GetPostingDataSize();
    size_t count = postings->postCount;
}
```

### Iterating Through Posting Lists

You can iterate through posting lists using the `GetCurrentWord` and `GetCurrentDoc` functions from the `SerializedPostingList` class. This provides an alternative way to access posts sequentially.

```cpp
// Example of iterating through word posts
const uint8_t* ptr = postings->GetPostingData();
Location currentLocation = 0;
WordPost* wordPost = nullptr;
while ((wordPost = postings->GetCurrentWord(&ptr, currentLocation)) != nullptr) {
    // Process the word post
    delete wordPost;
}

// Example of iterating through document posts
ptr = postings->GetPostingData();
currentLocation = 0;
DocumentPost* docPost = nullptr;
while ((docPost = postings->GetCurrentDoc(&ptr, currentLocation)) != nullptr) {
    // Process the document post
    delete docPost;
}
```

### Seeking Through Posts

```cpp
// For word posts
WordPost* post = postings->SeekWordPost(targetLocation, currentLocation);
if (post) {
    Location loc = post->startLocation;
    uint8_t flags = post->flags;
    delete post;
}

// For document posts
DocumentPost* doc = postings->SeekDocumentPost(targetLocation, prevEndLocation);
if (doc) {
    Location start = doc->startLocation;
    Location end = doc->endLocation;
    delete doc;
}
```

### URL Lookup

```cpp
// Get URL by ID
const char* url = blob->GetURL(urlId);
```

## Statistics and Metadata

The index maintains several statistics that can be accessed:

```cpp
// From Index object
Location totalWords = index.WordsInIndex;
Location totalDocs = index.DocumentsInIndex;
Location totalLocations = index.LocationsInIndex;
Location maxLocation = index.MaximumLocation;

// From serialized posting list
size_t postCount = postings->postCount;
size_t dataSize = postings->GetPostingDataSize();

// Document attributes
const DocumentAttributes* attrs = urlTable.GetDocumentAttributes(urlId);
if (attrs) {
    size_t wordCount = attrs->wordCount;
    size_t uniqueWordCount = attrs->uniqueWordCount;
    size_t documentLength = attrs->documentLength;
    const char* url = attrs->url;
}
```

## Important Notes

1. The index uses thread-safe operations for insertions
2. Memory management:
   - The index owns and manages memory for words and posting lists
   - Returned posts from Seek operations must be deleted by the caller
   - Blob and file resources should be properly cleaned up
3. Special features:
   - Title words are prefixed with '@'
   - Empty string "" is used as key for document posts
   - URLs are stored separately with their metadata

For more detailed information about specific components, refer to the header files:
- `Indexer.hpp`: Main indexer interface
- `Posts.hpp`: Posting list and serialization
- `HashBlob.h`: Blob storage and serialization 
