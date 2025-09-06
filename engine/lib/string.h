// string.h
//
// Starter file for a string template

#ifndef STRING_H
#define STRING_H

#include <cstddef>    // for size_t
#include <iostream>   // for ostream

#include "vector.h"


class string {
private:
    /**
     * storage for string characters
     * TAUTOLOGY: data_[data_.size()] == '\0'
     */
    vector<char> data_;

public:
    // Default Constructor
    // REQUIRES: Nothing
    // MODIFIES: *this
    // EFFECTS: Creates an empty string
    string()
        : data_(1, '\0') {}

    // Destructor
    ~string() = default;   // ensure cleanup happens via vector

    // string Literal / C string Constructor
    // REQUIRES: cstr is a null terminated C style string
    // MODIFIES: *this
    // EFFECTS: Creates a string with equivalent contents to cstr
    string(const char* cstr) {
        const char* end = cstr;
        while (*end != '\0') ++end;

        data_.reserve(static_cast<size_t>(end - cstr) + 1);

        for (const char* head = cstr; head != end; ++head) data_.pushBack(*head);

        data_.pushBack('\0');
    }

    string(const string& other) {
        data_.reserve(other.data_.size());

        for (size_t i = 0; i < other.size(); ++i)
            data_.pushBack(other.data_[i]);
    }
    string(string&& other) :
        data_{std::move(other.data_)}
    {}

    string(size_t size, char c) :
        data_(size + 1, c)
    {
        data_.back() = '\0';
    }

    void reserve(size_t capacity) {
        data_.reserve(capacity + 1);
    }

    // Size
    // REQUIRES: Nothing
    // MODIFIES: Nothing
    // EFFECTS: Returns the number of characters in the string
    size_t size() const { return data_.size() - 1; }

    // C string Conversion
    // REQUIRES: Nothing
    // MODIFIES: Nothing
    // EFFECTS: Returns a pointer to a null terminated C string of *this
    const char* cstr() const   // not sure
    {
        return data_.begin();
    }

    // Iterator Begin
    // REQUIRES: Nothing
    // MODIFIES: Nothing
    // EFFECTS: Returns a random access iterator to the start of the string
    const char* begin() const { return data_.begin(); }

    // Iterator End
    // REQUIRES: Nothing
    // MODIFIES: Nothing
    // EFFECTS: Returns a random access iterator to the end of the string
    const char* end() const { return (data_.begin() + (data_.size() - 1)); }

    // Element Access
    // REQUIRES: 0 <= i < size()
    // MODIFIES: Allows modification of the i'th element
    // EFFECTS: Returns the i'th character of the string
    char& operator[](size_t i) { return data_[i]; }

    // const ver
    const char& operator[](size_t i) const { return data_[i]; }

    // string Append
    // REQUIRES: Nothing
    // MODIFIES: *this
    // EFFECTS: Appends the contents of other to *this, resizing any
    //      memory at most once
    void operator+=(const string& other) {
        // space for characters of data_, characters of other, and the null terminator
        size_t newSize = data_.size() + other.size();
        if (data_.capacity() < newSize) {
            data_.reserve(newSize);
        }

        // remove our null terminator
        data_.popBack();

        for (size_t i = 0; i < other.size(); ++i) data_.pushBack(other[i]);

        data_.pushBack('\0');
    }


    // REQUIRES: cStr is a null terminated C style string
    // MODIFIES: *this
    // EFFECTS: Appends the contents of cStr to *this, resizing any
    //      memory at most once
    void operator+=(const char* cStr) {
        // Calculate the length of the C-string manually
        size_t cStrLen = 0;
        while (cStr[cStrLen] != '\0') {
            ++cStrLen;
        }

        // Calculate the new size
        size_t newSize = data_.size() + cStrLen;

        // Reserve space if necessary
        if (data_.capacity() < newSize) {
            data_.reserve(newSize);
        }

        // Remove the current null terminator from `data_`
        data_.popBack();

        // Append characters from the C-string
        for (size_t i = 0; i < cStrLen; ++i) {
            data_.pushBack(cStr[i]);
        }

        // Add the null terminator back
        data_.pushBack('\0');
    }

    // REQUIRES: cStr is a null terminated C style string
    // MODIFIES: *this
    // EFFECTS: Assigns the contents of cStr to *this, resizing any
    //      memory at most once
    string& operator=(const char* cStr) {
        // Calculate the length of the C-string
        size_t cStrLen = 0;
        while (cStr[cStrLen] != '\0') {
            ++cStrLen;
        }

        // Reserve space for the new string
        data_.clear();   // Clear existing data
        if (data_.capacity() < cStrLen + 1) {
            data_.reserve(cStrLen + 1);
        }

        // Copy characters from the C-string
        for (size_t i = 0; i < cStrLen; ++i) {
            data_.pushBack(cStr[i]);
        }

        // Add null terminator
        data_.pushBack('\0');

        return *this;
    }

    string& operator=(const string& other) {
        if (this != &other) {
            
            data_.clear();   // Clear existing data
            data_.reserve(other.data_.size());   // Reserve space for new string

            for (size_t i = 0; i < other.size(); ++i) {
                data_.pushBack(other[i]);
            }

        }
        return *this;
    }


    // Push Back
    // REQUIRES: Nothing
    // MODIFIES: *this
    // EFFECTS: Appends c to the string
    void pushBack(char c) {
        // overwrite null terminator
        data_.back() = c;
        // add null terminator
        data_.pushBack('\0');
    }

    // Pop Back
    // REQUIRES: string is not empty
    // MODIFIES: *this
    // EFFECTS: Removes the last charater of the string
    void popBack() {
        // remove null terminator
        data_.popBack();
        // replace last char with null terminator
        data_.back() = '\0';
    }

    struct equals_lex {
        bool operator()(char a, char b) const { return a == b; }
    };

    struct lesser_lex {
        bool operator()(char a, char b) const { return a < b; }
    };

    struct greater_lex {
        bool operator()(char a, char b) const { return b < a; }
    };


    template <typename Comp>
    bool comp_iterative(const string& other, const Comp& cmp) const {
        if (this == &other) {
            return cmp(data_[0], other[0]);
        }

        // Compare characters pair by pair
        for (size_t i = 0; i < std::min(this->size(), other.size()); ++i) {
            if (data_[i] != other[i]) {
                // Use the comparison functor for differing characters
                return cmp(data_[i], other[i]);
            }
        }
        // If one string is a prefix of the other, compare their sizes
        return cmp(this->size(), other.size());
    }


    // Equality Operator
    // REQUIRES: Nothing
    // MODIFIES: Nothing
    // EFFECTS: Returns whether all the contents of *this
    //    and other are equal
    bool operator==(const string& other) const {
        if (size() != other.size()) return false;

        return comp_iterative(other, equals_lex());
    }

    // Not-Equality Operator
    // REQUIRES: Nothing
    // MODIFIES: Nothing
    // EFFECTS: Returns whether at least one character differs between
    //    *this and other
    bool operator!=(const string& other) const { return !(*this == other); }

    // Less Than Operator
    // REQUIRES: Nothing
    // MODIFIES: Nothing
    // EFFECTS: Returns whether *this is lexigraphically less than other
    bool operator<(const string& other) const { return comp_iterative(other, lesser_lex()); }

    // Greater Than Operator
    // REQUIRES: Nothing
    // MODIFIES: Nothing
    // EFFECTS: Returns whether *this is lexigraphically greater than other
    bool operator>(const string& other) const { return comp_iterative(other, greater_lex()); }

    // Less Than Or Equal Operator
    // REQUIRES: Nothing
    // MODIFIES: Nothing
    // EFFECTS: Returns whether *this is lexigraphically less or equal to other
    bool operator<=(const string& other) const { return !(*this > other); }

    // Greater Than Or Equal Operator
    // REQUIRES: Nothing
    // MODIFIES: Nothing
    // EFFECTS: Returns whether *this is lexigraphically less or equal to other
    bool operator>=(const string& other) const { return !(*this < other); }
};

inline std::ostream& operator<<(std::ostream& os, const string& s) {
    for (char c : s) os << c;

    return os;
}
#endif   // STRING_H