#define _POSIX_C_SOURCE 200809L

#include "openember/osal/mutex.h"

#include <errno.h>
#include <pthread.h>
#include <stddef.h>
#include <string.h>

typedef struct {
    pthread_mutex_t native;
    int inited;
} oe_mutex_impl_t;

_Static_assert(sizeof(oe_mutex_impl_t) <= sizeof(oe_mutex_t), "oe_mutex_t opaque too small");

static oe_mutex_impl_t *impl(oe_mutex_t *m)
{
    return (oe_mutex_impl_t *)(void *)m;
}

oe_result_t oe_mutex_init(oe_mutex_t *mutex)
{
    int rc;

    if (!mutex) {
        return OE_ERR_INVALID_ARG;
    }

    memset(mutex, 0, sizeof(*mutex));
    rc = pthread_mutex_init(&impl(mutex)->native, NULL);
    if (rc != 0) {
        return OE_ERR_INTERNAL;
    }
    impl(mutex)->inited = 1;
    return OE_OK;
}

oe_result_t oe_mutex_destroy(oe_mutex_t *mutex)
{
    oe_mutex_impl_t *p;

    if (!mutex) {
        return OE_ERR_INVALID_ARG;
    }
    p = impl(mutex);
    if (!p->inited) {
        return OE_ERR_INVALID_ARG;
    }
    if (pthread_mutex_destroy(&p->native) != 0) {
        return OE_ERR_INTERNAL;
    }
    p->inited = 0;
    memset(mutex, 0, sizeof(*mutex));
    return OE_OK;
}

oe_result_t oe_mutex_lock(oe_mutex_t *mutex)
{
    int rc;

    if (!mutex || !impl(mutex)->inited) {
        return OE_ERR_INVALID_ARG;
    }
    rc = pthread_mutex_lock(&impl(mutex)->native);
    if (rc == EDEADLK) {
        return OE_ERR_DEADLOCK;
    }
    if (rc != 0) {
        return OE_ERR_INTERNAL;
    }
    return OE_OK;
}

oe_result_t oe_mutex_trylock(oe_mutex_t *mutex)
{
    int rc;

    if (!mutex || !impl(mutex)->inited) {
        return OE_ERR_INVALID_ARG;
    }
    rc = pthread_mutex_trylock(&impl(mutex)->native);
    if (rc == EBUSY) {
        return OE_ERR_AGAIN;
    }
    if (rc != 0) {
        return OE_ERR_INTERNAL;
    }
    return OE_OK;
}

oe_result_t oe_mutex_unlock(oe_mutex_t *mutex)
{
    if (!mutex || !impl(mutex)->inited) {
        return OE_ERR_INVALID_ARG;
    }
    if (pthread_mutex_unlock(&impl(mutex)->native) != 0) {
        return OE_ERR_INTERNAL;
    }
    return OE_OK;
}
