#pragma once
#include <pthread.h>
enum {
    PRIO_NONE = PTHREAD_PRIO_NONE,
    PRIO_INHERIT = PTHREAD_PRIO_INHERIT,
    EXPLICIT_SCHED = PTHREAD_EXPLICIT_SCHED
};

#if USE_PTHREADS

#define mutex_t pthread_mutex_t
#define mutex_init(m, attr) pthread_mutex_init(m, attr)
#define MUTEX_INITIALIZER PTHREAD_MUTEX_INITIALIZER
#define mutex_trylock(m) (!pthread_mutex_trylock(m))
#define mutex_lock pthread_mutex_lock
#define mutex_unlock pthread_mutex_unlock

#define mutexattr_t pthread_mutexattr_t
#define mutexattr_setprotocol pthread_mutexattr_setprotocol

#define PRIO_INHERIT PTHREAD_PRIO_INHERIT
#else

#include <stdbool.h>
#include <errno.h>
#include <stdint.h>

#include "atomic.h"
#include "futex.h"
#include "spinlock.h"

typedef struct {
    atomic int state;
    uint32_t __kind;
} mutex_t;

typedef struct {
    int type;
} mutexattr_t;

enum {
    MUTEX_LOCKED = 1 << 0,
    MUTEX_SLEEPING = 1 << 1,
};

#define MUTEX_INITIALIZER \
    {                     \
        .state = 0        \
    }
static const mutexattr_t default_mutexattr =
{
    .type = PRIO_NONE
};

static inline int mutex_init(mutex_t *mutex, mutexattr_t *attr)
{
    const mutexattr_t *imutexattr;
    atomic_init(&mutex->state, 0);
    imutexattr = ((const mutexattr_t *) attr ?: &default_mutexattr);
    store(&(mutex->__kind), imutexattr->type, relaxed);
    return 0;
}

static inline int mutexattr_setprotocol(mutexattr_t *mattr, int protocol)
{
    if (protocol != PRIO_NONE && protocol != PRIO_INHERIT)
        return EINVAL;
    mattr->type = protocol;
    return 0;
}

static bool mutex_trylock(mutex_t *mutex)
{
    int state, zero = 0;
    switch (mutex->__kind) {
    case PRIO_NONE:
        state = load(&mutex->state, relaxed);
        if (state & MUTEX_LOCKED)
            return false;
        break;
    case PRIO_INHERIT:
        if(!compare_exchange_weak(&mutex->state, &zero, gettid(), relaxed, relaxed)) {
            return false;
        }
        break;
    default:
        return false;
    }
    thread_fence(&mutex->state, acquire);
    return true;
}


static inline int mutex_lock(mutex_t *mutex)
{

#define MUTEX_SPINS 128
    int state = 0;
    for (int i = 0; i < MUTEX_SPINS; ++i) {
        if (mutex_trylock(mutex)) {
            return 0;
        }
        spin_hint();
    }

    switch (mutex->__kind) {
    case PRIO_NONE:
        state = exchange(&mutex->state, MUTEX_LOCKED | MUTEX_SLEEPING, relaxed);
        while (state & MUTEX_LOCKED) {
            futex_wait(&mutex->state, MUTEX_LOCKED | MUTEX_SLEEPING);
            state = exchange(&mutex->state, MUTEX_LOCKED | MUTEX_SLEEPING, relaxed);
        }
        break;
    case PRIO_INHERIT:
        futex_lock_pi(&mutex->state);
        break;
    default:
        return EINVAL;
    }

    thread_fence(&mutex->state, acquire);
    return 0;
}

static inline int mutex_unlock(mutex_t *mutex)
{
    int state, check, zero = 0;
    switch (mutex->__kind) {
    case PRIO_NONE:
        state = exchange(&mutex->state, 0, release);
        if (state & MUTEX_SLEEPING)
            futex_wake(&mutex->state, 1);
        break;
    case PRIO_INHERIT:
        check = gettid();
        if(!compare_exchange_weak(&mutex->state, &check, zero, relaxed, relaxed)) {
            futex_unlock_pi(&mutex->state);
        }
        return 0;
        break;
    default:
        return EINVAL;
    }
    return 0;
}

#endif