#define _POSIX_C_SOURCE 200809L

#include "openember/osal/sem.h"

#include <errno.h>
#include <semaphore.h>
#include <string.h>
#include <time.h>

typedef struct {
    sem_t native;
    int inited;
} oe_sem_impl_t;

_Static_assert(sizeof(oe_sem_impl_t) <= sizeof(oe_sem_t), "oe_sem_t opaque too small");

static oe_sem_impl_t *impl(oe_sem_t *s)
{
    return (oe_sem_impl_t *)(void *)s;
}

static oe_result_t abs_deadline_realtime_from_timeout_ms(int timeout_ms, struct timespec *out_ts)
{
    struct timespec now;
    long nsec;

    if (!out_ts) {
        return OE_ERR_INVALID_ARG;
    }
    if (clock_gettime(CLOCK_REALTIME, &now) != 0) {
        return OE_ERR_INTERNAL;
    }

    out_ts->tv_sec = now.tv_sec + (timeout_ms / 1000);
    nsec = now.tv_nsec + (long)(timeout_ms % 1000) * 1000000L;
    out_ts->tv_sec += nsec / 1000000000L;
    out_ts->tv_nsec = nsec % 1000000000L;
    return OE_OK;
}

oe_result_t oe_sem_init(oe_sem_t *s, uint32_t initial_count)
{
    if (!s) {
        return OE_ERR_INVALID_ARG;
    }

    memset(s, 0, sizeof(*s));
    if (sem_init(&impl(s)->native, 0, (unsigned)initial_count) != 0) {
        return OE_ERR_INTERNAL;
    }
    impl(s)->inited = 1;
    return OE_OK;
}

oe_result_t oe_sem_destroy(oe_sem_t *s)
{
    if (!s) {
        return OE_ERR_INVALID_ARG;
    }
    if (!impl(s)->inited) {
        memset(s, 0, sizeof(*s));
        return OE_OK;
    }
    if (sem_destroy(&impl(s)->native) != 0) {
        memset(s, 0, sizeof(*s));
        return OE_ERR_INTERNAL;
    }
    memset(s, 0, sizeof(*s));
    return OE_OK;
}

oe_result_t oe_sem_post(oe_sem_t *s)
{
    if (!s || !impl(s)->inited) {
        return OE_ERR_INVALID_ARG;
    }
    if (sem_post(&impl(s)->native) != 0) {
        return OE_ERR_INTERNAL;
    }
    return OE_OK;
}

oe_result_t oe_sem_wait(oe_sem_t *s)
{
    int rc;
    if (!s || !impl(s)->inited) {
        return OE_ERR_INVALID_ARG;
    }
    do {
        rc = sem_wait(&impl(s)->native);
    } while (rc != 0 && errno == EINTR);

    return (rc == 0) ? OE_OK : OE_ERR_INTERNAL;
}

oe_result_t oe_sem_trywait(oe_sem_t *s)
{
    if (!s || !impl(s)->inited) {
        return OE_ERR_INVALID_ARG;
    }
    if (sem_trywait(&impl(s)->native) == 0) {
        return OE_OK;
    }
    if (errno == EAGAIN) {
        return OE_ERR_AGAIN;
    }
    if (errno == EINTR) {
        return OE_ERR_AGAIN;
    }
    return OE_ERR_INTERNAL;
}

oe_result_t oe_sem_wait_timeout_ms(oe_sem_t *s, int timeout_ms)
{
    oe_result_t r;
    struct timespec abs_ts;
    int rc;

    if (!s || !impl(s)->inited) {
        return OE_ERR_INVALID_ARG;
    }
    if (timeout_ms < 0) {
        return oe_sem_wait(s);
    }
    if (timeout_ms == 0) {
        return oe_sem_trywait(s);
    }

    r = abs_deadline_realtime_from_timeout_ms(timeout_ms, &abs_ts);
    if (r != OE_OK) {
        return r;
    }

    do {
        rc = sem_timedwait(&impl(s)->native, &abs_ts);
    } while (rc != 0 && errno == EINTR);

    if (rc == 0) {
        return OE_OK;
    }
    if (errno == ETIMEDOUT) {
        return OE_ERR_TIMEOUT;
    }
    return OE_ERR_INTERNAL;
}

