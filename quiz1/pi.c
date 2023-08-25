#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <sched.h>

#include "atomic.h"
#include "mutex.h"

#define handle_error_en(en, msg) \
        do { errno = en; perror(msg); exit(EXIT_FAILURE); } while (0)

#define CHECK(func) \
        do { \
        int ret = 0; \
        if ((ret = func) != 0)\
            handle_error_en(ret, #func);\
        } while (0)

#define PRESET_PRIO

static void
display_sched_attr(int policy, struct sched_param *param)
{
    printf("    policy=%s, priority=%d\n",
            (policy == SCHED_FIFO)  ? "SCHED_FIFO" :
            (policy == SCHED_RR)    ? "SCHED_RR" :
            (policy == SCHED_OTHER) ? "SCHED_OTHER" :
            "???",
            param->sched_priority);
}

struct ctx {
    mutex_t m_t;
};

static void ctx_init(struct ctx *ctx)
{
    mutexattr_t mattr;
    CHECK(mutexattr_setprotocol(&mattr, PRIO_INHERIT));
    mutex_init(&ctx->m_t, &mattr);
}


static atomic bool busy_wait_test = true;
static atomic bool wait_order = true;

static void *thread_prilow(void *arg)
{
    struct ctx *ctx = (struct ctx *) arg;

    mutex_lock(&ctx->m_t);

    /* make sure low priority thread get lock first than high */
    store(&wait_order, false, relaxed);
    printf("start low pri\n");
    sleep(2);
    printf("end of busy work in low pri\n");
    mutex_unlock(&ctx->m_t);
    printf("low pri\n");
    return NULL;
}

static void *thread_primid(void *arg)
{
    while (load(&busy_wait_test, relaxed));
    printf("mid pri\n");
    return NULL;
}

static void *thread_prihigh(void *arg)
{
    struct ctx *ctx = (struct ctx *) arg;
    while (load(&wait_order, relaxed));
    printf("start high pri\n");
    mutex_lock(&ctx->m_t);
    if (load(&busy_wait_test, relaxed))
        printf("high pri inhro\n");

    mutex_unlock(&ctx->m_t);
    printf("high pri\n");
    return NULL;
}

int main()
{
    pthread_attr_t attr;
    pthread_t l_t, m_t, h_t;
    int policy;
    struct sched_param lpara, mpara, hpara, tmp;

    struct ctx ctx;
    ctx_init(&ctx);

    pthread_attr_init(&attr);
    CHECK(pthread_attr_setschedpolicy(&attr, SCHED_FIFO));
    CHECK(pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED));

    /* this is get main thread sched. policy */
    // struct sched_param main_para;
    // main_para.sched_priority = (sched_get_priority_min(SCHED_FIFO) +
    //                         sched_get_priority_max(SCHED_FIFO)) >> 1;
    // CHECK(pthread_setschedparam(pthread_self(), SCHED_FIFO, &main_para));
    // int policy;
    // pthread_getschedparam(pthread_self(), &policy, &main_para);
    // display_sched_attr(policy, &main_para);

    /* set priority with 2 ways:
       1. pre-set with the thread create
       2. create thread and adjust during execute. */
    lpara.sched_priority = sched_get_priority_min(SCHED_FIFO);
#ifdef PRESET_PRIO
    CHECK(pthread_attr_setschedparam(&attr, &lpara));
    CHECK(pthread_create(&l_t, &attr, &thread_prilow, &ctx));
#else
    CHECK(pthread_create(&l_t, NULL, &thread_prilow, &ctx));
    CHECK(pthread_setschedparam(l_t, SCHED_FIFO, &lpara));
#endif
    mpara.sched_priority = (sched_get_priority_min(SCHED_FIFO) +
                            sched_get_priority_max(SCHED_FIFO)) >> 1;
#ifdef PRESET_PRIO
    CHECK(pthread_attr_setschedparam(&attr, &mpara));
    CHECK(pthread_create(&m_t, &attr, &thread_primid, NULL));
#else
    CHECK(pthread_create(&m_t, NULL, &thread_primid, &ctx));
    CHECK(pthread_setschedparam(m_t, SCHED_FIFO, &mpara));
#endif

    hpara.sched_priority = sched_get_priority_max(SCHED_FIFO);
#ifdef PRESET_PRIO
    CHECK(pthread_attr_setschedparam(&attr, &hpara));
    CHECK(pthread_create(&h_t, &attr, &thread_prihigh, &ctx));
#else
    CHECK(pthread_create(&h_t, NULL, &thread_prihigh, &ctx));
    CHECK(pthread_setschedparam(h_t, SCHED_FIFO, &hpara));
#endif
    /* can't get high priority, because the high priority thread maybe terminate */
    pthread_getschedparam(m_t, &policy, &tmp);
    display_sched_attr(policy, &tmp);
    pthread_getschedparam(l_t, &policy, &tmp);
    display_sched_attr(policy, &tmp);

    sleep(4);
    store(&busy_wait_test, false, relaxed);

    pthread_join(l_t, NULL);
    pthread_join(m_t, NULL);
    pthread_join(h_t, NULL);
    pthread_attr_destroy(&attr);

    return 0;
}

