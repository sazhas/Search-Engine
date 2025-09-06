#ifndef UTILITY_H
#define UTILITY_H

#include <cassert>

/**
 * @brief A class that wraps a pointer to an object and allows for ambiguous return values;
 * @note by ambiguity we mean that the return value can be either a valid object or a null pointer;
 * @note the lifetime of the object is managed by this class;
 */
template <typename T>
class Ambiguous_Return {
private:
    
    T* elem;

public:

    Ambiguous_Return(T* elem) : 
        elem{elem} 
    {}

    bool is_null() const {
        return elem;
    }

    T& get() {
        assert(elem);
        return *elem;
    }

    const T& get() const {
        return get();
    }

    ~Ambiguous_Return() {
        if(elem)
            delete elem;
    }

}; /* class Ambiguous_Return<T> */

#endif /* UTILITY_H */