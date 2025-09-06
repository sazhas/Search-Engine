// HtmlParser.cpp
// Nicole Hamlton, nham@umich.edu

// If you don't define the HtmlParser class methods inline in
// HtmlParser.h, you may do it here.
#include "HtmlParser.h"
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <cstdlib>
#include <unistd.h>
#include <cstring>
#include <sstream>
#include <string>
#include "HtmlParser.h"
#include "HtmlTags.h"

using std::string;

//int debugCounter = 0;

inline void stringToLower(char* buffer, size_t length) {
    auto end = buffer + length;
    for (; buffer < end; ++buffer) {
        if ('A' <= *buffer && *buffer <= 'Z') {
            *buffer += 'a' - 'A';
        }
    }
}

inline bool IsWhitespace(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

inline uint8_t convert_flags(bool inBold, bool inHeading, bool inLargeFont) {
    uint8_t flags = 0;
    if (inBold) flags |= 0x01;
    if (inHeading) flags |= 0x02;
    if (inLargeFont) flags |= 0x04;
    return flags;
}

inline string ExtractAttribute(const string& tagContent, const string& attribute) {
    string key = attribute + "=\"";
    size_t start = tagContent.find(key);
    if (start != string::npos) {
        start += key.size();
        size_t end = tagContent.find('"', start);
        if (end != string::npos) {
            return tagContent.substr(start, end - start);
        }
    }
    return "";
}

const char* FindHrefAttribute(const char* ptr, const char* tagEnd) {
    const char* nextH = strchr(ptr, 'h');
    while (nextH && nextH < tagEnd) {
        if (strncmp(nextH, "href=\"", 6) == 0) {
            return nextH + 6;
        }
        nextH = strchr(nextH + 1, 'h');
    }
    return nullptr;
}

void HtmlParser::ParseTag(char*& ptr, string& tagDiscarding, bool& inTitle, bool& inAnchor, bool& inDiscardSection, bool& inHeading, bool& inBold, DesiredAction& discardType, string& currentLink) {
    pthread_testcancel();
    ++ptr;
    while (IsWhitespace(*ptr)) ++ptr;
    const char* start = ptr;
    while (*ptr && !IsWhitespace(*ptr) && *ptr != '>')
        ++ptr;

    // currently, closing tag names are as such "/head", "/div", thats fine but if they are recognized closing tags
    // they should not be treated the same as unrecognized closing tags
    string tagName(start, ptr - start);
    if (*start == '/'){
        tagName = string(start + 1, ptr - start - 1);
    }
    if (!tagName.empty() && tagName.back() == '/') {
        tagName.pop_back();
    }

    //std::cout << "tag found: " << tagName << '\n';
    DesiredAction action = LookupPossibleTag(tagName.c_str(), tagName.c_str() + tagName.length());

    if (tagName == "b") {
        inBold = true;
    }
    else if (tagName == "h1" || tagName == "h2" || tagName == "h3" || tagName == "h4" || tagName == "h5" || tagName == "h6") {
        inHeading = true;
    }

    //it is a recognized closing tag
    if (*start == '/' && action != DesiredAction::OrdinaryText){

        while (*ptr && *ptr != '>')
            ++ptr;
        if (*ptr == '>') ++ptr;
        return;
    }

    if (action == DesiredAction::DiscardSection){
        tagDiscarding = tagName;
        inDiscardSection = true, discardType = action;

        //std::cout << "entering discard section\n";
    }
    else if (action == DesiredAction::Comment){
        while (*ptr && !(ptr[0] == '-' && ptr[1] == '-' && ptr[2] == '>'))
            ++ptr;
        if (*ptr) ptr += 3;
        return;
    }
    else if (action == DesiredAction::Title){
        inTitle = true;
    }
    else if (action == DesiredAction::Anchor) {
        // Find the actual ending tag
        char* tagEnd = ptr;
        bool inQuotes = false;
        
        while (*tagEnd) {
            if (*tagEnd == '"') {
                inQuotes = !inQuotes;
            } else if (*tagEnd == '>' && !inQuotes) {
                break;
            }
            ++tagEnd;
        }
        if (*tagEnd == '>') {
            const char* hrefPos = FindHrefAttribute(ptr, tagEnd);
            if (hrefPos) {
                const char* endQuote = strchr(hrefPos, '"');
                if (endQuote && endQuote < tagEnd) {
                    std::string href(hrefPos, endQuote - hrefPos);
                    if (!href.empty()) {
                        links.emplace_back(href);
                        currentLink = href;
                        inAnchor = true;
                    }
                }
            }
        }
        ptr = tagEnd;
    }
    else if (action == DesiredAction::Base && base.empty()){
        const char* endPtr = strchr(ptr, '>');
        if (endPtr && *(endPtr - 1) == '/')
            --endPtr;
        base = ExtractAttribute(string(ptr, endPtr - ptr), "href");
        
    } 
    else if (action == DesiredAction::Embed) {
        auto endPtr = strchr(ptr, '>');
        if (!endPtr) {
            ptr = nullptr;
            return;
        }
        string src = ExtractAttribute(string(ptr, endPtr - ptr), "src");
        if (!src.empty())
            links.emplace_back(src);
    }
    else if (action == DesiredAction::OrdinaryText) {


        // FOR HANDLING BROKEN HTML
        // check if the tag is just literally unclosed
        // std::cout << "tag found, why are you in ordinary text: " << tagName << '\n';
        const char* tagStart = start - 1;
        const char* nextLT = strchr(ptr, '<');
        char* nextGT = strchr(ptr, '>');

        if (!nextGT || (nextLT && nextLT < nextGT)) {
            // add the broken UNCLOSED tag as a word or title word ig
            const char* lookBack = tagStart - 1;
            while (lookBack >= ptr - strlen(ptr) && !IsWhitespace(*lookBack) && *lookBack != '<') {
                --lookBack;
            }
            ++lookBack;
            std::string combinedWord;
            if (!words_flags.empty() && lookBack < tagStart) {
                combinedWord = std::string(lookBack, ptr - lookBack);
                words_flags.pop_back();
            } else {
                combinedWord = std::string(tagStart, ptr - tagStart);
            }
            //std::cout<<"word added from parse TAG" << combinedWord << '\n';
            if (inAnchor && !currentLink.empty()){
                if (links.back().anchorText.size())
                    links.back().anchorText.pop_back();
                links.back().anchorText.push_back(combinedWord);
            }
            if (inTitle) {
                titleWords.push_back(combinedWord);
            } else {
                words_flags.push_back(WFs(combinedWord, convert_flags(inBold, inHeading, false)));
            }


            // parse the rest of the text normally
            ParseText(ptr, inTitle, inAnchor, inHeading, inBold, currentLink);
            return;
        }
        else {
            std::string unrecognizedTag(tagStart, nextGT - tagStart + 1);
            std::istringstream stream(unrecognizedTag);
            std::string word;
            while (stream >> word) {
                if (inTitle) {
                    titleWords.push_back(word);
                } else {
                    words_flags.push_back(WFs(word, convert_flags(inBold, inHeading, false)));
                }
            }

            ptr = nextGT + 1;  // Move the pointer past the '>'
            return;
        }
    }
    else if (action == DesiredAction::HTML) {
        for (; *ptr && *ptr != '>'; ++ptr) {
            if (*ptr == 'l' && *(ptr + 1) == 'a' && *(ptr + 2) == 'n' && *(ptr + 3) == 'g' &&
                *(ptr + 4) == '=' && *(ptr + 5) == '"') {
                if (*(ptr + 6) == 'e' && *(ptr + 7) == 'n') {
                    english = true;
                } else {
                    english = false;
                }
                ptr += 8;
                break;
            }
        }
    }
    //go to end of tag name
    while (*ptr && *ptr != '>')
        ++ptr;
    if (*ptr == '>') ++ptr;
}

inline void HtmlParser::ParseText(char*& ptr, bool inTitle, bool inAnchor, bool inHeading, bool inBold, const string& currentLink) {
    const char* start = ptr;

    while (*ptr && *ptr != '<') {
        if (IsWhitespace(*ptr)) {
            if (start != ptr) {
                std::string word(start, ptr - start);
                if (inAnchor && !currentLink.empty()) {
                    links.back().anchorText.push_back(word);
                }
                if (inTitle) {
                    titleWords.push_back(word);
                } else {
                    words_flags.push_back(WFs(word, convert_flags(inBold, inHeading, false)));
                }
            }
            ++ptr;
            while (IsWhitespace(*ptr)) ++ptr;
            start = ptr;
        } else {
            ++ptr;
        }
    }
    if (start != ptr) {
        std::string word(start, ptr - start);
        if (inAnchor && !currentLink.empty()) {
            links.back().anchorText.push_back(word);
        }
        if (inTitle) {
            titleWords.push_back(word);
        } else {
            words_flags.push_back(WFs(word, convert_flags(inBold, inHeading, false)));
        }
    }
}

inline char* FindFirstClosingTag(char* ptr, const char*& tagType, string& tagDiscarding, int& tagLength) {
    char* nextTag = strchr(ptr, '<');
    while (nextTag) {
        if (strncmp(nextTag, "</script>", 9) == 0 && "script" == tagDiscarding) {
            tagType = "</script>";
            tagLength = 9;
            return nextTag;
        }
        if (strncmp(nextTag, "</style>", 8) == 0 && "style" == tagDiscarding) {
            tagType = "</style>";
            tagLength = 8;
            return nextTag;
        }
        if (strncmp(nextTag, "</svg>", 6) == 0 && "svg" == tagDiscarding) {
            tagType = "</svg>";
            tagLength = 6;
            return nextTag;
        }
	    // if (strncmp(nextTag, "</bsp-jw-player>", 16) == 0 && "bsp-jw-player" == tagDiscarding) {
        //     tagType = "</bsp-jw-player>";
        //     tagLength = 16;
        //     return nextTag;
        // }
        nextTag = strchr(nextTag + 1, '<');
    }
    tagType = nullptr;
    tagLength = 0;
    return nullptr;
}

HtmlParser::HtmlParser(char* buffer, size_t length) {
    char* ptr = buffer;
    stringToLower(buffer, length);
    std::string tagDiscarding = "";
    bool inTitle = false, inAnchor = false, inDiscardSection = false, inHeading = false, inBold = false;
    DesiredAction discardType;
    string currentLink;

    while (ptr && buffer <= ptr && ptr < buffer + length) {
        pthread_testcancel();
        if (*ptr == '<') {
            if (ptr[1] == '/' && inTitle && strncmp(ptr + 2, "title", 5) == 0){
                // close title tag
                inTitle = false;
                ptr = strchr(ptr, '>') + 1;
            }
            else if (ptr[1] == '/' && inAnchor && strncmp(ptr + 2, "a", 1) == 0){
                // close anchor tag
                inAnchor = false;
                ptr = strchr(ptr, '>') + 1;
            }
            else if (ptr[1] == '/' && inHeading && ptr[2] == 'h' && ptr[3] >= '1' && ptr[3] <= '6'){
                // close heading tag
                inHeading = false;
                ptr = strchr(ptr, '>') + 1;
            }
            else if (ptr[1] == '/' && inBold && strncmp(ptr + 2, "b", 1) == 0){
                // close bold tag
                inBold = false;
                ptr = strchr(ptr, '>') + 1;
                if (!ptr) {
                    break;
                }
            }
            else if (inDiscardSection) {
                // how the discard section is exited
                const char* tagType = nullptr;
                int tagLength = 0;
                // std::cout << "handling discard section" << tagType << "\n";
                char* closestEnd = FindFirstClosingTag(ptr, tagType, tagDiscarding, tagLength);
                if (closestEnd) {
                    ptr = closestEnd + tagLength;
                    // std::cout << "leaving discard section\n";
                    inDiscardSection = false;
                } else {
                    break;
                }
            } else {
                ParseTag(ptr, tagDiscarding, inTitle, inAnchor, inDiscardSection, inHeading, inBold, discardType, currentLink);
            }
        } else {
            if (!inDiscardSection){
                ParseText(ptr, inTitle, inAnchor, inHeading, inBold, currentLink);
            }
            else{
                // in discard section, continue
                ++ptr;
            }
        }
    }

    // combine vector of title words into one string
    if (titleWords.size() > 0) {
        title_chunk = titleWords[0];
        for (size_t i = 1; i < titleWords.size(); ++i) {
            title_chunk += " " + titleWords[i];
        }
    }

    //std::cout << "debug counter " << debugCounter << '\n';
}
