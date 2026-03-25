#define _POSIX_C_SOURCE 200809L

#include "openember/osal/thread.h"

#include <errno.h>
#include <pthread.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

struct oe_thread_start {
    oe_thread_fn fn;
    void *arg;
};

typedef struct {
    pthread_t handle;
    int started;
    int joined;
} oe_thread_impl_t;

_Static_assert(sizeof(oe_thread_impl_t) <= sizeof(oe_thread_t), "oe_thread_t opaque too small");

static oe_thread_impl_t *impl(oe_thread_t *t)
{
    return (oe_thread_impl_t *)(void *)t;
}

static void *oe_thread_trampoline(void *p)
{
    struct oe_thread_start *s = p;
    oe_thread_fn fn;
    void *arg;

    if (!s) {
        return NULL;
    }
    fn = s->fn;
    arg = s->arg;
    free(s);
    if (fn) {
        fn(arg);
    }
    return NULL;
}

oe_result_t oe_thread_create(oe_thread_t *thread, oe_thread_fn fn, void *arg)
{
    struct oe_thread_start *start;
    int rc;

    if (!thread || !fn) {
        return OE_ERR_INVALID_ARG;
    }

    memset(thread, 0, sizeof(*thread));
    start = (struct oe_thread_start *)malloc(sizeof(*start));
    if (!start) {
        return OE_ERR_NOMEM;
    }
    start->fn = fn;
    start->arg = arg;

    rc = pthread_create(&impl(thread)->handle, NULL, oe_thread_trampoline, start);
    if (rc != 0) {
        free(start);
        return OE_ERR_INTERNAL;
    }
    impl(thread)->started = 1;
    impl(thread)->joined = 0;
    return OE_OK;
}

oe_result_t oe_thread_join(oe_thread_t *thread)
{
    oe_thread_impl_t *p;
    int rc;

    if (!thread) {
        return OE_ERR_INVALID_ARG;
    }
    p = impl(thread);
    if (!p->started) {
        return OE_ERR_INVALID_ARG;
    }
    if (p->joined) {
        return OE_ERR_INVALID_ARG;
    }

    rc = pthread_join(p->handle, NULL);
    if (rc != 0) {
        return OE_ERR_INTERNAL;
    }
    p->joined = 1;
    memset(thread, 0, sizeof(*thread));
    return OE_OK;
}
