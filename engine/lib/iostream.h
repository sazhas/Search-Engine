#ifndef IRS_IOSTREAM
#define IRS_IOSTREAM

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>

#include <unistd.h>

#include "constants.h"

namespace irs {
class ostream {
public:
    ostream(int fd)
        : _fd(fd) {}
    ~ostream() { flush(); }

    inline ostream& operator<<(const std::string& str) {
        writeToBuffer(str.c_str(), str.size());
        return *this;
    }

    inline ostream& operator<<(const char* str) {
        writeToBuffer(str, strlen(str));
        return *this;
    }

    inline ostream& operator<<(const char c) {
        writeToBuffer(&c, 1);
        return *this;
    }

    inline ostream& operator<<(const bool b) {
        if (b) {
            writeToBuffer("true", 4);
        } else {
            writeToBuffer("false", 5);
        }
        return *this;
    }

    inline ostream& operator<<(const int32_t num) {
        char buf[20];
        int len = intToStr(num, buf);
        writeToBuffer(buf, len);
        return *this;
    }

    inline ostream& operator<<(const int64_t num) {
        char buf[20];
        int len = intToStr(num, buf);
        writeToBuffer(buf, len);
        return *this;
    }

    inline ostream& operator<<(const uint32_t num) {
        char buf[20];
        int len = uintToStr(num, buf);
        writeToBuffer(buf, len);
        return *this;
    }

    inline ostream& operator<<(const uint64_t num) {
        char buf[20];
        int len = uintToStr(num, buf);
        writeToBuffer(buf, len);
        return *this;
    }

    inline ostream& operator<<(ostream& (*func)(ostream&) ) { return func(*this); }

    inline void flush() {
        if (buffer_len > 0) {
            write(_fd, buffer, buffer_len);
            buffer_len = 0;
        }
    }

private:
    int _fd;
    char buffer[COUT_BUFFER_SIZE];
    size_t buffer_len;

    inline void writeToBuffer(const char* data, size_t len) {
        if (buffer_len + len >= COUT_BUFFER_SIZE) {
            flush();
        }

        if (len >= COUT_BUFFER_SIZE) {
            write(_fd, data, len);
            return;
        }

        memcpy(buffer + buffer_len, data, len);
        buffer_len += len;
    }

    inline static int intToStr(int64_t num, char* buf) {
        bool neg = false;
        int i = 0;

        if (num == 0) {
            buf[i++] = '0';
            return i;
        }

        if (num < 0) {
            neg = true;
            num = -num;
        }

        while (num > 0) {
            buf[i++] = '0' + (num % 10);
            num /= 10;
        }

        if (neg) {
            buf[i++] = '-';
        }

        for (int j = 0; j < i / 2; ++j) {
            char temp = buf[j];
            buf[j] = buf[i - j - 1];
            buf[i - j - 1] = temp;
        }

        return i;
    }

    inline static int uintToStr(uint64_t num, char* buf) {
        int i = 0;

        if (num == 0) {
            buf[i++] = '0';
            return i;
        }

        while (num > 0) {
            buf[i++] = '0' + (num % 10);
            num /= 10;
        }

        for (int j = 0; j < i / 2; ++j) {
            char temp = buf[j];
            buf[j] = buf[i - j - 1];
            buf[i - j - 1] = temp;
        }

        return i;
    }
};

inline ostream& endl(ostream& out) {
    out << '\n';
    out.flush();
    return out;
}

inline ostream cout(STDOUT_FILENO);
inline ostream cerr(STDERR_FILENO);
}   // namespace irs

#endif
