#pragma once

#include <pthread.h>
#include "mutex.h"

class CV {
public:
    inline CV() {
        pthread_cond_init(&_cv, nullptr);
    }

    inline void wait(Mutex& mutex) {
        pthread_cond_wait(&_cv, &mutex._mutex);
    }

    inline void broadcast() {
        pthread_cond_broadcast(&_cv);
    }

    inline void signal() {
        pthread_cond_signal(&_cv);
    }

    inline ~CV() {
        pthread_cond_destroy(&_cv);
    }

private:
    pthread_cond_t _cv;
};
