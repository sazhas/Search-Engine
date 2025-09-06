#ifndef THREAD_H
#define THREAD_H

#include <pthread.h>

class Thread {
protected:

    pthread_t thread;
    bool started;

public:



};

class Thread_Joinable : public Thread {

};

#endif /* THREAD_H */