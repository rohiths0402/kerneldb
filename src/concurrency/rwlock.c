#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include "rwlock.h"

RWLockResult rwlock_init(RWLock *rw) {
    if (!rw) return RWLOCK_ERROR;
    memset(rw, 0, sizeof(RWLock));
    if (pthread_rwlock_init(&rw->lock, NULL) != 0) {
        perror("pthread_rwlock_init");
        return RWLOCK_ERROR;
    }
    rw->readers = 0;
    rw->writers = 0;
    return RWLOCK_OK;
}

void rwlock_destroy(RWLock *rw) {
    if (!rw) return;
    pthread_rwlock_destroy(&rw->lock);
    rw->readers = 0;
    rw->writers = 0;
}

RWLockResult rwlock_read_lock(RWLock *rw) {
    if (!rw) return RWLOCK_ERROR;
    if (pthread_rwlock_rdlock(&rw->lock) != 0) {
        perror("pthread_rwlock_rdlock");
        return RWLOCK_ERROR;
    }
    __sync_fetch_and_add(&rw->readers, 1);
    return RWLOCK_OK;
}

void rwlock_read_unlock(RWLock *rw) {
    if (!rw) return;
    __sync_fetch_and_sub(&rw->readers, 1);
    pthread_rwlock_unlock(&rw->lock);
}

RWLockResult rwlock_write_lock(RWLock *rw) {
    if (!rw) return RWLOCK_ERROR;

    if (pthread_rwlock_wrlock(&rw->lock) != 0) {
        perror("pthread_rwlock_wrlock");
        return RWLOCK_ERROR;
    }

    __sync_fetch_and_add(&rw->writers, 1);
    return RWLOCK_OK;
}

void rwlock_write_unlock(RWLock *rw) {
    if (!rw) return;
    __sync_fetch_and_sub(&rw->writers, 1);
    pthread_rwlock_unlock(&rw->lock);
}

void rwlock_print(const RWLock *rw) {
    if (!rw) return;
    printf("  [rwlock] readers=%d writers=%d\n", rw->readers, rw->writers);
}