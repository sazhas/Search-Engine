#ifndef DEBUG_H
#define DEBUG_H

#ifndef NDEBUG
#define PRINTF(...) fprintf(stderr, __VA_ARGS__);
#else
#define PRINTF(...) 
#endif

#endif /* DEBUG_H */