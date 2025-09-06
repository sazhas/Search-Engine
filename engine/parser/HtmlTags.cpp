#include "HtmlTags.h"

#include <cassert>
#include <cstring>
#include <iostream>

#include <ctype.h>

template <typename T>
inline T Min(const T& a, const T& b) {
    return (a < b) ? a : b;
}

inline char ToLower(char c) {
    return 'A' <= c && c <= 'Z' ? c + 'a' - 'A' : c;
}

inline int CStringComp(const char* s1, const char* s2, size_t N) {
    int count = 0;
    while (count < N && *s1 && *s2) {
        char c1 = ToLower(*s1);
        char c2 = *s2;

        if (c1 != c2) break;

        ++s1;
        ++s2;
        ++count;
    }
    return (unsigned char) ToLower(*s1) - (unsigned char) (*s2);
}

DesiredAction LookupPossibleTag(const char* name, const char* nameEnd) {
    size_t tagLength = nameEnd - name;

    if (tagLength > LongestTagLength) {
        return DesiredAction::OrdinaryText;
    }

    int left = 0;
    int right = NumberOfTags - 1;

    while (left <= right) {
        int middle = left + (right - left) / 2;

        size_t length = std::min(tagLength, strlen(TagsRecognized[middle].Tag));

        int comp = CStringComp(name, TagsRecognized[middle].Tag, length);

        if (comp == 0 && tagLength == strlen(TagsRecognized[middle].Tag)) {
            return TagsRecognized[middle].Action;
        } else if (comp > 0) {
            left = middle + 1;
        } else {
            right = middle - 1;
        }
    }


    return DesiredAction::Discard;
}
