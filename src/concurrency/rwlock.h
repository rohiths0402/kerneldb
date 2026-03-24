#ifndef RWLOCK_H
#define RWLOCK_H

#include <pthread.h>

typedef struct {
    pthread_rwlock_t lock;
    int readers;
    int writers;
} RWLock;

typedef enum {
    RWLOCK_OK,
    RWLOCK_ERROR
} RWLockResult;

RWLockResult rwlock_init(RWLock *rw);
void rwlock_destroy(RWLock *rw);
RWLockResult rwlock_read_lock(RWLock *rw);
void rwlock_read_unlock(RWLock *rw);
RWLockResult rwlock_write_lock(RWLock *rw);
void rwlock_write_unlock(RWLock *rw);
void rwlock_print(const RWLock *rw);

#endif