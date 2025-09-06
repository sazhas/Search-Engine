#ifndef DYNAMIC_BITSET_H
#define DYNAMIC_BITSET_H

#include <cstdint> 
#include <cstddef>
#include <cstring>
#include <cassert>
#include <unistd.h>

class Dynamic_Bitset {
private:

    size_t size;
    uint8_t* data;

    inline constexpr static size_t min_multiple_of_8_geq_than(size_t n) {
        return (n + 7) & ~7;
    }

    inline constexpr static size_t max_multiple_of_8_leq_than(size_t n) {
        return n & ~7;
    }
    
public:

    Dynamic_Bitset() :
        size{0},
        data{nullptr}
    {}

    /**
     * @brief Construct a new Dynamic Bitset object
     * @param size The size of the bitset in bits represented.
     */
    Dynamic_Bitset(size_t size) :
        size{size},
        data{new uint8_t[min_multiple_of_8_geq_than(size) >> 3]}
    {
        for(size_t i = 0; i < min_multiple_of_8_geq_than(size) >> 3; ++i)
            data[i] = 0;
    }

    Dynamic_Bitset(const Dynamic_Bitset& other) :
        size{other.size},
        data{new uint8_t[min_multiple_of_8_geq_than(size) >> 3]}
    {
        memcpy(data, other.data, min_multiple_of_8_geq_than(size) >> 3);
    }

    Dynamic_Bitset(Dynamic_Bitset&& other) :
        size{other.size},
        data{other.data}
    {
        other.size = 0;
        other.data = nullptr;
    }

    Dynamic_Bitset& operator=(const Dynamic_Bitset& other) {
        if(this == &other)
            return *this;

        size = other.size;
        delete[] data;
        data = new uint8_t[min_multiple_of_8_geq_than(size) >> 3];
        memcpy(data, other.data, min_multiple_of_8_geq_than(size) >> 3);

        return *this;
    }

    Dynamic_Bitset& operator=(Dynamic_Bitset&& other) {
        if(this == &other)
            return *this;

        size = other.size;
        delete[] data;
        data = other.data;
        other.size = 0;
        other.data = nullptr;

        return *this;
    }

    ~Dynamic_Bitset() {
        delete[] data;
    }

    size_t get_size() const {
        return size;
    }

    void resize(size_t size_new) {
        uint8_t* data_new = new uint8_t[min_multiple_of_8_geq_than(size_new) >> 3];
        memcpy(data_new, data, min_multiple_of_8_geq_than(size) >> 3);
        delete[] data;
        data = data_new;
        size = size_new;
    }

    bool get_bit(size_t idx) const {
        assert(idx < size);

        size_t segment_bit = max_multiple_of_8_leq_than(idx);
        size_t offset = idx - segment_bit;

        size_t segment = segment_bit >> 3;

        return data[segment] & (1 << offset);
    }

    void set_bit_true(size_t idx) {
        assert(idx < size);

        size_t segment_bit = max_multiple_of_8_leq_than(idx);
        size_t offset = idx - segment_bit;

        size_t segment = segment_bit >> 3;

        data[segment] |= (1 << offset);
    }

    void set_bit_false(size_t idx) {
        assert(idx < size);

        size_t segment_bit = max_multiple_of_8_leq_than(idx);
        size_t offset = idx - segment_bit;

        size_t segment = segment_bit >> 3;

        data[segment] &= ~(1 << offset);
    }
    
    /**
     * @brief Flips the bit at the given index.
     * @return The new value of the bit.
     */
    bool flip_bit(size_t idx) {
        assert(idx < size);

        size_t segment_bit = max_multiple_of_8_leq_than(idx);
        size_t offset = idx - segment_bit;

        size_t segment = segment_bit >> 3;

        data[segment] ^= (1 << offset);
        return data[segment] & (1 << offset);
    }

    void read_from_file(int fd) {
        read(fd, &size, sizeof(size));
        delete[] data;
        data = new uint8_t[min_multiple_of_8_geq_than(size) >> 3];
        read(fd, data, min_multiple_of_8_geq_than(size) >> 3);
    }

    void write_to_file(int fd) {
        write(fd, &size, sizeof(size));
        write(fd, data, min_multiple_of_8_geq_than(size) >> 3);
    }

}; /* Dynamic_Bitset */

#endif /* DYNAMIC_BITSET_H */
