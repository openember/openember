#define _POSIX_C_SOURCE 200809L

#include "openember/osal/cond.h"

#include <errno.h>
#include <pthread.h>
#include <string.h>
#include <time.h>

typedef struct {
    pthread_cond_t native;
    int inited;
} oe_cond_impl_t;

_Static_assert(sizeof(oe_cond_impl_t) <= sizeof(oe_cond_t), "oe_cond_t opaque too small");

static oe_cond_impl_t *impl(oe_cond_t *c)
{
    return (oe_cond_impl_t *)(void *)c;
}

/* oe_mutex.c defines its own internal impl; we replicate the minimal layout here. */
typedef struct {
    pthread_mutex_t native;
    int inited;
} oe_mutex_impl_for_cond_t;

_Static_assert(sizeof(oe_mutex_impl_for_cond_t) <= sizeof(oe_mutex_t), "oe_mutex_t opaque too small (cond)");

static oe_mutex_impl_for_cond_t *mutex_impl(oe_mutex_t *m)
{
    return (oe_mutex_impl_for_cond_t *)(void *)m;
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

oe_result_t oe_cond_init(oe_cond_t *c)
{
    int rc;

    if (!c) {
        return OE_ERR_INVALID_ARG;
    }
    memset(c, 0, sizeof(*c));
    rc = pthread_cond_init(&impl(c)->native, NULL);
    if (rc != 0) {
        return OE_ERR_INTERNAL;
    }
    impl(c)->inited = 1;
    return OE_OK;
}

oe_result_t oe_cond_destroy(oe_cond_t *c)
{
    int rc;

    if (!c) {
        return OE_ERR_INVALID_ARG;
    }
    if (!impl(c)->inited) {
        memset(c, 0, sizeof(*c));
        return OE_OK;
    }
    rc = pthread_cond_destroy(&impl(c)->native);
    memset(c, 0, sizeof(*c));
    if (rc != 0) {
        return OE_ERR_INTERNAL;
    }
    return OE_OK;
}

oe_result_t oe_cond_wait(oe_cond_t *c, oe_mutex_t *mutex)
{
    int rc;

    if (!c || !mutex) {
        return OE_ERR_INVALID_ARG;
    }
    if (!impl(c)->inited || !mutex_impl(mutex)->inited) {
        return OE_ERR_INVALID_ARG;
    }

    rc = pthread_cond_wait(&impl(c)->native, &mutex_impl(mutex)->native);
    if (rc != 0) {
        return OE_ERR_INTERNAL;
    }
    return OE_OK;
}

oe_result_t oe_cond_wait_timeout_ms(oe_cond_t *c, oe_mutex_t *mutex, int timeout_ms)
{
    int rc;
    struct timespec abs_ts;
    oe_result_t r;

    if (!c || !mutex) {
        return OE_ERR_INVALID_ARG;
    }
    if (timeout_ms < 0) {
        return oe_cond_wait(c, mutex);
    }

    if (!impl(c)->inited || !mutex_impl(mutex)->inited) {
        return OE_ERR_INVALID_ARG;
    }

    r = abs_deadline_from_timeout_ms(timeout_ms, &abs_ts);
    if (r != OE_OK) {
        return r;
    }

#if defined(_POSIX_TIMEOUTS) && (_POSIX_TIMEOUTS >= 200112L)
    rc = pthread_cond_timedwait(&impl(c)->native, &mutex_impl(mutex)->native, &abs_ts);
#else
    rc = pthread_cond_timedwait(&impl(c)->native, &mutex_impl(mutex)->native, &abs_ts);
#endif

    if (rc == 0) {
        return OE_OK;
    }
    if (rc == ETIMEDOUT) {
        return OE_ERR_TIMEOUT;
    }
    return OE_ERR_INTERNAL;
}

oe_result_t oe_cond_signal(oe_cond_t *c)
{
    int rc;

    if (!c) {
        return OE_ERR_INVALID_ARG;
    }
    if (!impl(c)->inited) {
        return OE_ERR_INVALID_ARG;
    }
    rc = pthread_cond_signal(&impl(c)->native);
    return (rc == 0) ? OE_OK : OE_ERR_INTERNAL;
}

oe_result_t oe_cond_broadcast(oe_cond_t *c)
{
    int rc;

    if (!c) {
        return OE_ERR_INVALID_ARG;
    }
    if (!impl(c)->inited) {
        return OE_ERR_INVALID_ARG;
    }
    rc = pthread_cond_broadcast(&impl(c)->native);
    return (rc == 0) ? OE_OK : OE_ERR_INTERNAL;
}

