#ifndef ALGORITHM_H
#define ALGORITHM_H

/* TODO: make this multithreaded and/or cannibalizing of c1 and c2 */
#include <cstddef>
#include <string>
#include <vector>

#include <arpa/inet.h>

template <typename C1, typename C2, typename C_Out>
C_Out merge_sorted(const C1& c1, const C2& c2) {
    C_Out ret;
    ret.reserve(c1.size() + c2.size());

    auto it1 = c1.begin();
    auto it2 = c2.begin();

    bool contained = true;
    while (contained) {
        if (it1 == c1.end() && it2 == c2.end())
            contained = false;
        else if (it1 == c1.end()) {
            ret.pushBack(*it2);
            ++it2;

        } else if (it2 == c2.end()) {
            ret.pushBack(*it1);
            ++it1;

        } else {
            auto v1 = *it1;
            auto v2 = *it2;

            if (v1 < v2) {
                ret.pushBack(v1);
                ret.pushBack(v2);
                ++it1;
            } else {
                ret.pushBack(v2);
                ret.pushBack(v1);
                ++it2;
            }
        }
    }

    return ret;
}

inline uint32_t final_mix(uint32_t h) {
    h ^= h >> 16;
    h *= 0x85ebca6b;
    h ^= h >> 13;
    h *= 0xc2b2ae35;
    h ^= h >> 16;
    return h;
}

inline uint32_t hash_url(const std::string& url) {
    uint32_t h = 2166136261u;
    for (char c : url) {
        h ^= static_cast<uint32_t>(c);
        h *= 16777619u;
    }
    return final_mix(h);
}
template <typename T>
void insert_sorted(std::vector<T>& vec, const T& value) {
    // Binary search using custom comparator
    std::size_t low = 0, high = vec.size();
    while (low < high) {
        std::size_t mid = low + (high - low) / 2;
        if (vec[mid] < value)
            low = mid + 1;
        else
            high = mid;
    }

    // Insert at the correct position
    vec.push_back(T());   // grow the vector
    for (std::size_t i = vec.size() - 1; i > low; --i) {
        vec[i] = vec[i - 1];
    }
    vec[low] = value;
}


// Round up to the next multiple of boundary (boundary must be a power of 2)
// Consistent with the implementation in HashBlob and URLBlob
inline size_t RoundUp(size_t length, size_t boundary) {
    static const size_t oneless = boundary - 1, mask = ~(oneless);
    return (length + oneless) & mask;
}

template <typename T>
inline const T& my_min(const T& a, const T& b) {
    return (a < b) ? a : b;
}

template <typename InputIt, typename OutputIt, typename UnaryOperation>
OutputIt my_transform(InputIt first, InputIt last, OutputIt d_first, UnaryOperation unary_op) {
    while (first != last) {
        *d_first++ = unary_op(*first++);
    }
    return d_first;
}

inline void to_lowercase(std::string& input) {
    for (char& c : input) {
        if (c >= 'A' && c <= 'Z') {
            c += 'a' - 'A';   // or c += 32;
        }
    }
}

inline double custom_exp(double x) {
    // Clamp to avoid extreme values
    if (x > 20.0) return 4.85e8;     // exp(20)
    if (x < -20.0) return 2.06e-9;   // exp(-20)

    // Use symmetry: exp(-x) = 1 / exp(x)
    bool negative = false;
    if (x < 0) {
        negative = true;
        x = -x;
    }

    // Horner's method for 7-term expansion
    double result
      = 1.0 + x * (1.0 + x * (0.5 + x * (1.0 / 6.0 + x * (1.0 / 24.0 + x * (1.0 / 120.0 + x * (1.0 / 720.0))))));

    return negative ? 1.0 / result : result;
}

#if __BYTE_ORDER == __LITTLE_ENDIAN
inline uint64_t my_htonll(uint64_t val) {
    return (((uint64_t) htonl(val & 0xFFFFFFFF)) << 32) | htonl(val >> 32);
}
inline uint64_t my_ntohll(uint64_t val) {
    return (((uint64_t) ntohl(val & 0xFFFFFFFF)) << 32) | ntohl(val >> 32);
}
#else
inline uint64_t my_htonll(uint64_t val) {
    return val;
}
inline uint64_t my_ntohll(uint64_t val) {
    return val;
}
#endif

#endif /* ALGORITHM_H */
