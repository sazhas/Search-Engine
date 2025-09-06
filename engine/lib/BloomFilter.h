#ifndef BLOOMFILTER_H
#define BLOOMFILTER_H

#include <fcntl.h>
#include <unistd.h>
#include <vector>
#include <cmath>
#include <string>
#include <string.h>
#include <openssl/md5.h>

#include "dynamic_bitset.h"

class Bloomfilter
{
    using Bitset = Dynamic_Bitset;

private:
    size_t num_bits;
    size_t num_hashes;
    Bitset data;

    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    std::pair<uint64_t, uint64_t> hash(const std::string &datum)
    {
        // Use MD5 to hash the string into one 128 bit hash, and split into 2 hashes.
        unsigned char md[MD5_DIGEST_LENGTH];
        MD5(reinterpret_cast<const unsigned char *>(datum.c_str()), datum.size(), md);

        uint64_t hash1 = 0;
        uint64_t hash2 = 0;

        std::memcpy(&hash1, md, sizeof(uint64_t));
        std::memcpy(&hash2, md + sizeof(uint64_t), sizeof(uint64_t));

        return std::make_pair(hash1, hash2);
    }
    #pragma GCC diagnostic pop

    std::vector<size_t> double_hash(const std::string &datum)
    {
        std::pair<uint64_t, uint64_t> hashes = hash(datum);

        std::vector<size_t> indices;
        indices.reserve(num_hashes);

        for (size_t i = 0; i < num_hashes; ++i)
        {
            size_t index = (hashes.first + i * hashes.second) % num_bits;
            indices.push_back(index);
        }

        return indices;
    }

public:

    Bloomfilter(int num_objects, double false_positive_rate) {
        // Determine the size of bits of our data vector, and resize.
        double m_double = -1 * (num_objects * std::log(false_positive_rate) / (std::log(2) * std::log(2)));

        size_t m = static_cast<size_t>(std::ceil(m_double));

        num_bits = m;

        data = Bitset(num_bits);

        // Determine number of hash functions to use.
        double k_double = (m_double / num_objects) * std::log(2);

        size_t k = static_cast<size_t>(std::ceil(k_double));

        num_hashes = k;
    }

    Bloomfilter(const char* filename) : num_bits(0), num_hashes(0), data() {
        auto filter_fd = open(filename, O_RDONLY);
        if (filter_fd == -1) {
            return;
        }

        read(filter_fd, &num_bits, sizeof(num_bits));
        read(filter_fd, &num_hashes, sizeof(num_hashes));

        data.read_from_file(filter_fd);

        close(filter_fd);
    }

    void insert(const std::string &s)
    {
        // Hash the string into two unique hashes.

        // Use double hashing to get unique bit, and repeat for each hash function.

        auto indices = double_hash(s);

        for (const auto i : indices)
            data.set_bit_true(i);
    }

    bool contains(const std::string &s)
    {
        // Hash the string into two unqiue hashes.

        // Use double hashing to get unique bit, and repeat for each hash function.
        // If bit is false, we know for certain this unique string has not been inserted.

        // If all bits were true, the string is likely inserted, but false positive is possible.

        auto indices = double_hash(s);

        for (const auto i : indices)
            if (!data.get_bit(i))
                return false;

        return true;
    }

    void save(const char* filename) {
        auto filter_fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (filter_fd == -1) {
            return;
        }

        write(filter_fd, &num_bits, sizeof(num_bits));
        write(filter_fd, &num_hashes, sizeof(num_hashes));

        data.write_to_file(filter_fd);

        close(filter_fd);
    }
};

#endif
