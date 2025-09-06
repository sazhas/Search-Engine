#pragma once

#include <pthread.h>
#include <cstddef>
#include <cassert>

class CV;

class Mutex {
public:
    inline Mutex() {
        pthread_mutex_init(&_mutex, nullptr);
    };

    inline void lock() {
        pthread_mutex_lock(&_mutex);
    }

    inline void unlock() {
        pthread_mutex_unlock(&_mutex);
    }

    inline ~Mutex() {
        pthread_mutex_destroy(&_mutex);
    }

private:
    pthread_mutex_t _mutex;

    friend class CV;
}; /* class Mutex */

class Mutex_Shared {
private:

    pthread_rwlock_t lock;

public:

    inline Mutex_Shared() {
        pthread_rwlock_init(&lock, nullptr);
    }

    inline ~Mutex_Shared() {
        pthread_rwlock_destroy(&lock);
    }

    inline void lock_read() {
        pthread_rwlock_rdlock(&lock);
    }

    inline void lock_write() {
        pthread_rwlock_wrlock(&lock);
    }

    inline void unlock() {
        pthread_rwlock_unlock(&lock);
    }

}; /* Mutex_Shared */

template <typename Mut, void (Mut::*Func_Lock)()>
class Lock_Guard {
private:

    Mut& mutex;
    bool is_locked;

public:

    Lock_Guard(Mut& mutex) : 
        mutex{mutex},
        is_locked{true}
    {
        (mutex.*Func_Lock)();
    }

    ~Lock_Guard() {
        if (is_locked)
            mutex.unlock();
    }

    void lock() {
        assert(!is_locked);
        (mutex.*Func_Lock)();
        is_locked = true;
    }

    void unlock() {
        assert(is_locked);
        mutex.unlock();
        is_locked = false;
    }
};

using Lock_Guard_Write = Lock_Guard<Mutex_Shared, &Mutex_Shared::lock_write>;
using Lock_Guard_Read = Lock_Guard<Mutex_Shared, &Mutex_Shared::lock_read>;
