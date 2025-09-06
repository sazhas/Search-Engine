// vector.h
//
// Starter file for a vector template
#ifndef VECTOR_H
#define VECTOR_H


// included for size_t
#include <cassert>
#include <cstddef>
#include <initializer_list>
#include <iostream>
#include <new>
#include <type_traits>
#include <utility>

template <typename T>
class vector {
private:
    template <typename U = T, typename Ret = void>
    using Typ_Fund = typename std::enable_if<std::is_fundamental<U>::value, Ret>::type;

    template <typename U = T, typename Ret = void>
    using Typ_NFund = typename std::enable_if<!std::is_fundamental<U>::value, Ret>::type;

    template <typename U = T>
    static Typ_Fund<U, U*> allocate_array_compile(size_t elems) {
        U* memory = new U[elems];
        return memory;
    }

    template <typename U = T>
    static Typ_NFund<U, U*> allocate_array_compile(size_t elems) {
        void* memory = operator new[](elems * sizeof(U));
        return (U*) memory;
    }

    /**
     * @brief allocates a raw memory region for an array of type T with elems elements
     * @param elems number of elements this array memory region can hold
     * @returns a T pointer to the start of the array
     */
    static T* allocate_array(size_t elems) { return allocate_array_compile(elems); }

    template <typename U = T>
    static Typ_Fund<U, void> free_array_compile(U*& arr) {
        if (arr) {
            delete[] arr;
            arr = nullptr;
        }
    }

    template <typename U = T>
    static Typ_NFund<U, void> free_array_compile(U*& arr) {
        if (arr) {
            operator delete[]((void*) arr);
            arr = nullptr;
        }
    }

    /**
     * @brief frees a raw memory region for an array of type T that was initialized by
     * allocate_array
     * @param arr pointer to the start of the memory region
     * @post arr is set to nullptr
     */
    static void free_array(T*& arr) { free_array_compile(arr); }

    template <typename U = T>
    Typ_Fund<U, void> scrap_data_compile() {
        return;
    }

    template <typename U = T>
    Typ_NFund<U, void> scrap_data_compile() {
        for (size_t i = 0; i < size_max_attained; ++i) data_[i].~U();
    }

    void scrap_data() {
        scrap_data_compile();

        size_ = 0;
        size_max_attained = 0;

        free_array(data_);
        capacity_ = 0;
    }

    /**
     * @brief computes the minimum power of 2 greater than n; if n is a power of 2
     * then we get its double
     */
    static size_t find_least_power_of_2(size_t n) {
        size_t least = 1;
        while (least <= n) least *= 2;
        return least;
    }

    template <typename U = T>
    Typ_Fund<U, void> clone_from_compile(const vector<U>& other) {
        for (size_t i = 0; i < size_; ++i) data_[i] = other.data_[i];
    }

    template <typename U = T>
    Typ_NFund<U, void> clone_from_compile(const vector<U>& other) {
        for (size_t i = 0; i < size_; ++i) new (data_ + i) U(other.data_[i]);
    }

    /**
     * @brief deep copies other to *this, setting the relevant members
     */
    void clone_from(const vector<T>& other) {
        capacity_ = other.capacity_;
        size_ = other.size_;
        size_max_attained = other.size_;
        data_ = allocate_array(capacity_);

        clone_from_compile(other);
    }

    /**
     * @brief transfers the resources of other to *this, leaves other in
     * a default constructed state
     */
    void cannibalize(vector<T>&& other) {
        data_ = other.data_;
        capacity_ = other.capacity_;
        size_ = other.size_;
        size_max_attained = other.size_max_attained;

        other.data_ = nullptr;
        other.size_ = 0;
        other.size_max_attained = 0;
        other.capacity_ = 0;
    }

public:
    // Default Constructor
    // REQUIRES: Nothing
    // MODIFIES: *this
    // EFFECTS: Constructs an empty vector with capacity 0
    vector()
        : data_ { nullptr }
        , size_ { 0 }
        , size_max_attained { 0 }
        , capacity_ { 0 } {}

private:
    template <typename U = T>
    Typ_Fund<U, void> DTOR_compile() {
        return;
    }

    template <typename U = T>
    Typ_NFund<U, void> DTOR_compile() {
        /*
            explicitly call destructor for all elements due to usage of placement new
            decoupling the DTOR from memory management

            size_max_attained is required here because we do not actually scrub the memory
            of elements when they are popped in order to optimize
        */
        for (size_t i = 0; i < size_max_attained; ++i) data_[i].~U();
    }

public:
    // Destructor
    // REQUIRES: Nothing
    // MODIFIES: Destroys *this
    // EFFECTS: Performs any necessary clean-up operations
    ~vector() {
        DTOR_compile();
        free_array(data_);
    }

private:
    template <typename U = T>
    Typ_Fund<U, void> init_ptr_offset_with_val(U*& ptr, size_t offset, const T& val) {
        ptr[offset] = val;
    }

    template <typename U = T>
    Typ_NFund<U, void> init_ptr_offset_with_val(U*& ptr, size_t offset, const T& val) {
        new (ptr + offset) U(val);
    }

public:
    // Iterator Range Constructor
    vector(const T* begin, const T* end)
        : data_ { nullptr }
        , size_ { static_cast<size_t>(end - begin) }
        , size_max_attained { static_cast<size_t>(end - begin) }
        , capacity_ { find_least_power_of_2(static_cast<size_t>(end - begin)) } {
        data_ = allocate_array(capacity_);

        for (size_t i = 0; i < static_cast<size_t>(end - begin); i++) {
            auto ptr = begin + i;
            init_ptr_offset_with_val(data_, i, *ptr);
        }
    }

    // Initializer-List Constructor
    vector(const std::initializer_list<T>& list)
        : data_ { nullptr }
        , size_ { list.size() }
        , size_max_attained { list.size() }
        , capacity_ { find_least_power_of_2(list.size()) } {
        data_ = allocate_array(capacity_);

        size_t i = 0;
        for (const auto& v : list) {
            init_ptr_offset_with_val(data_, i, v);
            i++;
        }
    }

    // Fill Constructor
    // REQUIRES: Capacity > 0
    // MODIFIES: *this
    // EFFECTS: Creates a vector with size num_elements, all assigned to val
    vector(size_t num_elements, const T& val)
        : data_ { nullptr }
        , size_(num_elements)
        , size_max_attained { num_elements }
        , capacity_ { find_least_power_of_2(num_elements) } {
        data_ = allocate_array(capacity_);

        /* in-place copy construct Ts, from val, at each location */
        for (size_t i = 0; i < size_; ++i) init_ptr_offset_with_val(data_, i, val);
    }

    // Resize Constructor
    // REQUIRES: Nothing
    // MODIFIES: *this
    // EFFECTS: Constructs a vector with size num_elements,
    //    all default constructed
    vector(size_t num_elements)
        : vector(num_elements, T()) {}

    // Copy Constructor
    // REQUIRES: Nothing
    // MODIFIES: *this
    // EFFECTS: Creates a clone of the vector other
    vector(const vector<T>& other) { clone_from(other); }

    // Assignment operator
    // REQUIRES: Nothing
    // MODIFIES: *this
    // EFFECTS: Duplicates the state of other to *this
    vector<T>& operator=(const vector<T>& other) {
        if (this != &other) {
            scrap_data();
            clone_from(other);
        }
        return *this;
    }

    // Move Constructor
    // REQUIRES: Nothing
    // MODIFIES: *this, leaves other in a default constructed state
    // EFFECTS: Takes the data from other into a newly constructed vector
    vector(vector<T>&& other) { cannibalize(std::move(other)); }

    // Move Assignment Operator
    // REQUIRES: Nothing
    // MODIFIES: *this, leaves other in a default constructed state
    // EFFECTS: Takes the data from other in constant time
    vector<T>& operator=(vector<T>&& other) {
        if (this != &other) {
            scrap_data();
            cannibalize(std::move(other));
        }
        return *this;
    }

    // REQUIRES: new_capacity > capacity( )
    // MODIFIES: capacity( )
    // EFFECTS: Ensures that the vector can contain size( ) = new_capacity
    //    elements before having to reallocate
    void reserve(size_t newCapacity) {
        assert(newCapacity > capacity());

        auto data_new = allocate_array(newCapacity);

        for (size_t i = 0; i < size_; ++i) init_ptr_offset_with_val(data_new, i, data_[i]);

        scrap_data_compile();
        free_array(data_);

        data_ = data_new;
        size_max_attained = size_;
        capacity_ = newCapacity;
    }

    // REQUIRES: Nothing
    // MODIFIES: Nothing
    // EFFECTS: Returns the number of elements in the vector
    size_t size() const { return size_; }

    // REQUIRES: Nothing
    // MODIFIES: Nothing
    // EFFECTS: Returns the maximum size the vector can attain before resizing
    size_t capacity() const { return capacity_; }

    // REQUIRES: 0 <= i < size( )
    // MODIFIES: Allows modification of data[i]
    // EFFECTS: Returns a mutable reference to the i'th element
    T& operator[](size_t i) { return data_[i]; }

    // REQUIRES: 0 <= i < size( )
    // MODIFIES: Nothing
    // EFFECTS: Get a const reference to the ith element
    const T& operator[](size_t i) const { return data_[i]; }

private:
    template <typename U = T>
    Typ_Fund<U, void> pushBack_compile(const U& x) {
        data_[size_++] = x;
        if (size_max_attained < size_) size_max_attained = size_;
    }

    template <typename U = T>
    Typ_NFund<U, void> pushBack_compile(const U& x) {
        if (
          /* we are in memory without constructed objects */
          size_ == size_max_attained) {
            new (data_ + size_) U(x);
            size_max_attained = ++size_;
        } else {
            /*
                we can directly reference data_[size_] as a T;
                invokes move operator= and cannibalizes
            */
            data_[size_++] = U(x);
        }
    }

public:
    // REQUIRES: Nothing
    // MODIFIES: this, size( ), capacity( )
    // EFFECTS: Appends the element x to the vector, allocating
    //    additional space if necessary
    void pushBack(const T& x) {
        // Check if `x` is within the bounds of `data_`
        bool x_is_in_bounds = (data_ <= &x) && (&x < data_ + size_);
        T temp = x_is_in_bounds ? x : T();   // Default initialize if not in bounds
        const T& value_to_push = x_is_in_bounds ? temp : x;

        if (size_ == capacity_) reserve(find_least_power_of_2(capacity_));

        pushBack_compile(x);
    }

    // REQUIRES: Nothing
    // MODIFIES: this, size( )
    // EFFECTS: Removes the last element of the vector,
    //    leaving capacity unchanged
    void popBack() {
        if (size_ > 0) --size_;
    }

    void clear() {
        if (size_ > 0) size_ = 0;
    }

    // REQUIRES: Nothing
    // MODIFIES: Allows mutable access to the vector's contents
    // EFFECTS: Returns a mutable random access iterator to the
    //    first element of the vector
    T* begin() { return data_; }

    // REQUIRES: Nothing
    // MODIFIES: Allows mutable access to the vector's contents
    // EFFECTS: Returns a mutable random access iterator to
    //    one past the last valid element of the vector
    T* end() { return data_ ? (data_ + size_) : data_; }

    // REQUIRES: Nothing
    // MODIFIES: Nothing
    // EFFECTS: Returns a random access iterator to the first element of the vector
    const T* begin() const { return data_; }

    // REQUIRES: Nothing
    // MODIFIES: Nothing
    // EFFECTS: Returns a random access iterator to
    //    one past the last valid element of the vector
    const T* end() const {
        if (!data_) return nullptr;
        return data_ + size_;
    }

    T& front() { return *data_; }

    const T& front() const { return *data_; }

    T& back() { return *(data_ + (size_ - 1)); }

    const T& back() const { return *(data_ + (size_ - 1)); }

private:            // not sure what else to add
    T* data_;       // points to the dynamic array
    size_t size_;   // current num#ber of elements

    /*
       Maximum size we have attained; we track this because popBack
       has an optimization where we only reduce the bounds of the
       array while not actually removing the objects from memory,
       meaning some memory regions are initialized to objects while
       others are just raw memory. These regions need to be treated
       differently when written to.
    */
    size_t size_max_attained;

    size_t capacity_;   // Allocated (size + remaining) memory capacity
};

#endif   // VECTOR_H