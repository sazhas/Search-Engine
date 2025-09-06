#include <iostream>
#include <vector>

#include <unistd.h>

#include "../indexer/Indexer.hpp"
#include "HtmlParser.h"
#include "Parser.h"

Parser* globalParser = nullptr;

int main() {
    int port = 1024;

    Parser* globalParserInstance = new Parser(port);
    globalParser = globalParserInstance;

    Index index;

    // -------------------------------
    // FAKE PARSER DATA TO TEST INDEX
    // -------------------------------
    HtmlParser* fake1 = new HtmlParser("", 0);
    fake1->pageURL = "http://test1.com";
    fake1->titleWords = { "motherfucking" };
    fake1->words = { "this", "is", "a", "test", "motherfucking" };

    HtmlParser* fake2 = new HtmlParser("", 0);
    fake2->pageURL = "http://test2.com";
    fake2->titleWords = { "another" };
    fake2->words = { "motherfucking", "test", "again" };

    {
        Lock_Guard<Mutex, &Mutex::lock> guard(globalParserInstance->parsedPagesLock);
        globalParserInstance->parsedPages.push_back(fake1);
        globalParserInstance->parsedPages.push_back(fake2);
    }

    // Simulate processing parsed URLs
    std::vector<HtmlParser*> toProcess;
    {
        Lock_Guard<Mutex, &Mutex::lock> guard(globalParserInstance->parsedPagesLock);
        toProcess.swap(globalParserInstance->parsedPages);
    }

    std::cout << "WRITING TO BLOB" << std::endl;

    for (HtmlParser* parserObj : toProcess) {
        if (parserObj) {
            index.Insert(parserObj, const_cast<char*>(parserObj->pageURL.c_str()));
            std::cout << "Inserting " << parserObj->pageURL << std::endl;
            delete parserObj;
        }
    }

    // --------------------------------
    // BUILD INDEX BLOB + TEST LOOKUP
    // --------------------------------
    auto blob = IndexBlob::Create(&index);
    auto list = blob->Find("motherfucking");
    if (list != nullptr) {
        std::cout << "[FOUND] Posting list for 'motherfucking': " << list->GetPostingDataSize() << " bytes\n";
    } else {
        std::cout << "[MISSING] 'motherfucking' not found in the index.\n";
    }
    IndexBlob::Discard(blob);

    return 0;
}