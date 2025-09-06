#ifndef CIRCULAR_BUFFER_H
#define CIRCULAR_BUFFER_H

#include <cstddef>
#include <utility>
#include "mutex.h"
#include "cv.h"
#include "utility.h"

/**
 * @brief A thread-safe circular buffer that supports head insertion and tail removal
 * @warning T must be default constructible and move-assignable
 */
template <typename T>
class Extrema_Circular_Buffer {
private:

    T* data;
    size_t capacity;
    #define CAPACITY_ACTUAL (capacity + 1)

    size_t idx_head, idx_next_empty;

    mutable CV cv_poppable;
    mutable Mutex mut_poppable;

    mutable CV cv_pushable;
    mutable Mutex mut_pushable;

    /**
     * @pre appropriate locks must be acquired before calling this function
     * @pre size == capacity
     */
    void resize_and_copy() {
        assert(full_nonlocking());

        size_t new_capacity = 2 * capacity;
        T* new_data = new T[new_capacity + 1];

        size_t idx_zeroed_old = idx_head;
        size_t idx_zeroed_new = 0;
        while(idx_zeroed_old != idx_next_empty) {
            new_data[idx_zeroed_new] = data[idx_zeroed_old];
            idx_zeroed_old = (idx_zeroed_old + 1) % CAPACITY_ACTUAL;
            ++idx_zeroed_new;
        }

        delete[] data;
        data = new_data;
        capacity = new_capacity;
        idx_head = 0;
        idx_next_empty = idx_zeroed_new;
    }

    size_t idx_next(size_t idx) const {
        return (idx + 1) % CAPACITY_ACTUAL;
    }

    size_t size_nonlocking() const {
        return (idx_next_empty - idx_head + CAPACITY_ACTUAL) % CAPACITY_ACTUAL;
    }

    bool empty_nonlocking() const {
        return idx_head == idx_next_empty;
    }

    bool full_nonlocking() const {
        return (idx_next_empty + 1) % CAPACITY_ACTUAL == idx_head;
    }

public:

    Extrema_Circular_Buffer(size_t capacity) :
        data{new T[CAPACITY_ACTUAL]},
        capacity{capacity},
        idx_head{0},
        idx_next_empty{0}
    {}

    ~Extrema_Circular_Buffer() {}

    size_t size() const {
        Lock_Guard<Mutex, &Mutex::lock> g0{mut_poppable};
        Lock_Guard<Mutex, &Mutex::lock> g1{mut_pushable};

        return size_nonlocking();
    }

    bool empty() const {
        Lock_Guard<Mutex, &Mutex::lock> g0{mut_poppable};
        Lock_Guard<Mutex, &Mutex::lock> g1{mut_pushable};

        return empty_nonlocking();
    }

    bool full() const {
        Lock_Guard<Mutex, &Mutex::lock> g0{mut_poppable};
        Lock_Guard<Mutex, &Mutex::lock> g1{mut_pushable};

        return full_nonlocking();
    }

    /**
     * @warning blocks if there are no elements
     */
    const T& front() const {
        Lock_Guard<Mutex, &Mutex::lock> g0{mut_poppable};

        return data[idx_head];
    }

    /**
     * @brief runs a function pred on the front element and pops it if pred returns true; if there is no front element the function blocks
     * @return an Ambiguous_Return object that contains the front element if pred returned true, otherwise nullptr
     * @param pred (T&) -> bool
     * @warning the behavior of pred must keep the object in a valid state
     */
    template <typename Pred>
    Ambiguous_Return<T> pop_and_get_front_if_pred(const Pred& pred) {
        Lock_Guard<Mutex, &Mutex::lock> g0{mut_poppable};
        Lock_Guard<Mutex, &Mutex::lock> g1{mut_pushable};

        /* wait for the signal for an available element */
        while(empty_nonlocking()) {
            g1.unlock();
            cv_poppable.wait(mut_poppable);
            g1.lock();
        }

        g1.unlock();

        auto& front = data[idx_head];

        if(pred(front)) {            
            Ambiguous_Return<T> enclosed{new T{std::move(front)}};

            /* invalidate current head */
            idx_head = idx_next(idx_head);

            g0.unlock();

            /* signal a thread waiting for an empty spot */
            g1.lock();

            cv_pushable.signal();

            return enclosed;
            // unlock by RAII for g1
        } else {
            return Ambiguous_Return<T>{nullptr};
            // unlock by RAII for g0
        }
    }

    T pop_and_get_front() {
        Ambiguous_Return<T> ret = pop_and_get_front_if_pred(
            [](const T& elem) {
                return true;
            }   
        );

        return ret.get();
    }

    void insert_tail_blocking(const T& elem) {
        Lock_Guard<Mutex, &Mutex::lock> g0{mut_poppable};
        Lock_Guard<Mutex, &Mutex::lock> g1{mut_pushable};

        /* wait for the signal for an empty spot */
        while(full_nonlocking()) {
            g1.unlock();
            cv_pushable.wait(mut_poppable);
            g1.lock();
        }

        /* unlock the head for pops */
        g0.unlock();

        data[idx_next_empty] = elem;
        idx_next_empty = idx_next(idx_next_empty);

        g1.unlock();

        /* signal a thread waiting for an available element */
        g0.lock();

        cv_poppable.signal();

        // unlock by RAII for g0
    }

    void insert_tail_resizing(const T& elem) {
        Lock_Guard<Mutex, &Mutex::lock> g0{mut_poppable};
        Lock_Guard<Mutex, &Mutex::lock> g1{mut_pushable};

        if(full_nonlocking()) {
            resize_and_copy();
            cv_pushable.broadcast();
        }

        // unlock head for pops
        g0.unlock();

        data[idx_next_empty] = elem;
        idx_next_empty = idx_next(idx_next_empty);

        g1.unlock();

        /* signal a thread waiting for an available element */
        g0.lock();

        cv_poppable.signal();

        // unlock by RAII for g0
    }

}; /* class Extrema_Circular_Buffer<T> */

template <typename T>
using Circular_Buffer = Extrema_Circular_Buffer<T>;

#endif /* CIRCULAR_BUFFER_H */
