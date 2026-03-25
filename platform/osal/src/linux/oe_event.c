#define _POSIX_C_SOURCE 200809L

#include "openember/osal/event.h"

#include <errno.h>
#include <pthread.h>
#include <string.h>
#include <time.h>

typedef struct {
    pthread_mutex_t m;
    pthread_cond_t c;
    int signaled;
    int inited;
} oe_event_impl_t;

_Static_assert(sizeof(oe_event_impl_t) <= sizeof(oe_event_t), "oe_event_t opaque too small");

static oe_event_impl_t *impl(oe_event_t *e)
{
    return (oe_event_impl_t *)(void *)e;
}

static oe_result_t abs_deadline_from_timeout_ms(int timeout_ms, struct timespec *out_ts)
{
    struct timespec now;
    long nsec;

    if (!out_ts) {
        return OE_ERR_INVALID_ARG;
    }
    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) {
        return OE_ERR_INTERNAL;
    }

    out_ts->tv_sec = now.tv_sec + (timeout_ms / 1000);
    nsec = now.tv_nsec + (long)(timeout_ms % 1000) * 1000000L;
    out_ts->tv_sec += nsec / 1000000000L;
    out_ts->tv_nsec = nsec % 1000000000L;
    return OE_OK;
}

oe_result_t oe_event_init(oe_event_t *e)
{
    if (!e) {
        return OE_ERR_INVALID_ARG;
    }
    memset(e, 0, sizeof(*e));
    if (pthread_mutex_init(&impl(e)->m, NULL) != 0) {
        return OE_ERR_INTERNAL;
    }
    if (pthread_cond_init(&impl(e)->c, NULL) != 0) {
        pthread_mutex_destroy(&impl(e)->m);
        return OE_ERR_INTERNAL;
    }
    impl(e)->signaled = 0;
    impl(e)->inited = 1;
    return OE_OK;
}

oe_result_t oe_event_destroy(oe_event_t *e)
{
    if (!e) {
        return OE_ERR_INVALID_ARG;
    }
    if (!impl(e)->inited) {
        memset(e, 0, sizeof(*e));
        return OE_OK;
    }
    (void)pthread_cond_destroy(&impl(e)->c);
    (void)pthread_mutex_destroy(&impl(e)->m);
    memset(e, 0, sizeof(*e));
    return OE_OK;
}

oe_result_t oe_event_set(oe_event_t *e)
{
    if (!e || !impl(e)->inited) {
        return OE_ERR_INVALID_ARG;
    }
    if (pthread_mutex_lock(&impl(e)->m) != 0) {
        return OE_ERR_INTERNAL;
    }
    impl(e)->signaled = 1;
    (void)pthread_cond_signal(&impl(e)->c);
    (void)pthread_mutex_unlock(&impl(e)->m);
    return OE_OK;
}

oe_result_t oe_event_reset(oe_event_t *e)
{
    if (!e || !impl(e)->inited) {
        return OE_ERR_INVALID_ARG;
    }
    if (pthread_mutex_lock(&impl(e)->m) != 0) {
        return OE_ERR_INTERNAL;
    }
    impl(e)->signaled = 0;
    (void)pthread_mutex_unlock(&impl(e)->m);
    return OE_OK;
}

oe_result_t oe_event_wait(oe_event_t *e)
{
    oe_result_t r;
    if (!e) {
        return OE_ERR_INVALID_ARG;
    }
    r = oe_event_wait_timeout_ms(e, -1);
    return r;
}

oe_result_t oe_event_wait_timeout_ms(oe_event_t *e, int timeout_ms)
{
    int rc = 0;
    struct timespec abs_ts;
    oe_result_t r;

    if (!e || !impl(e)->inited) {
        return OE_ERR_INVALID_ARG;
    }

    if (pthread_mutex_lock(&impl(e)->m) != 0) {
        return OE_ERR_INTERNAL;
    }

    if (timeout_ms < 0) {
        while (!impl(e)->signaled) {
            rc = pthread_cond_wait(&impl(e)->c, &impl(e)->m);
            if (rc != 0) {
                (void)pthread_mutex_unlock(&impl(e)->m);
                return OE_ERR_INTERNAL;
            }
        }
    } else {
        r = abs_deadline_from_timeout_ms(timeout_ms, &abs_ts);
        if (r != OE_OK) {
            (void)pthread_mutex_unlock(&impl(e)->m);
            return r;
        }

        while (!impl(e)->signaled) {
            rc = pthread_cond_timedwait(&impl(e)->c, &impl(e)->m, &abs_ts);
            if (rc == ETIMEDOUT) {
                (void)pthread_mutex_unlock(&impl(e)->m);
                return OE_ERR_TIMEOUT;
            }
            if (rc != 0) {
                (void)pthread_mutex_unlock(&impl(e)->m);
                return OE_ERR_INTERNAL;
            }
        }
    }

    /* auto-reset */
    impl(e)->signaled = 0;
    (void)pthread_mutex_unlock(&impl(e)->m);
    return OE_OK;
}

