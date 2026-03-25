#define _GNU_SOURCE

#include "openember/osal/mq.h"

#include <errno.h>
#include <mqueue.h>
#include <poll.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

typedef struct {
    mqd_t mq;
    int inited;
    size_t msg_size;
} oe_mq_impl_t;

_Static_assert(sizeof(oe_mq_impl_t) <= sizeof(oe_mq_t), "oe_mq_t opaque too small (mq)");

static oe_mq_impl_t *impl(oe_mq_t *mq)
{
    return (oe_mq_impl_t *)(void *)mq->opaque;
}

static oe_result_t abs_deadline_ms(int timeout_ms, struct timespec *out_ts)
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

oe_result_t oe_mq_query_caps(oe_mq_caps_t *out_caps)
{
    if (!out_caps) {
        return OE_ERR_INVALID_ARG;
    }
    out_caps->supports_message_queue = 1u;
    out_caps->max_msg_size = 0;
    out_caps->max_msgs = 0;
    return OE_OK;
}

oe_result_t oe_mq_create(oe_mq_t *mq,
                           const char *name,
                           uint32_t max_msgs,
                           uint32_t msg_size)
{
    struct mq_attr attr;
    oe_mq_impl_t *p;

    if (!mq || !name || max_msgs == 0 || msg_size == 0) {
        return OE_ERR_INVALID_ARG;
    }

    memset(mq, 0, sizeof(*mq));
    p = impl(mq);
    p->mq = (mqd_t)-1;
    p->inited = 0;
    p->msg_size = msg_size;

    memset(&attr, 0, sizeof(attr));
    attr.mq_maxmsg = (long)max_msgs;
    attr.mq_msgsize = (long)msg_size;
    attr.mq_flags = 0; /* blocking; we handle timeout via mq_timedsend/receive */

    mqd_t mqd = mq_open(name, O_CREAT | O_EXCL | O_RDWR, 0666, &attr);
    if (mqd < 0) {
        if (errno == ENOSYS || errno == ENOTSUP) {
            return OE_ERR_UNSUPPORTED;
        }
        if (errno == ENOENT || errno == EACCES) {
            /* Common when /dev/mqueue is not available or permissions are restricted */
            return OE_ERR_UNSUPPORTED;
        }
        if (errno == EEXIST) {
            return OE_ERR_BUSY;
        }
        return OE_ERR_INTERNAL;
    }

    p->mq = mqd;
    p->inited = 1;
    return OE_OK;
}

oe_result_t oe_mq_open(oe_mq_t *mq, const char *name)
{
    oe_mq_impl_t *p;

    if (!mq || !name) {
        return OE_ERR_INVALID_ARG;
    }

    memset(mq, 0, sizeof(*mq));
    p = impl(mq);
    p->mq = (mqd_t)-1;
    p->inited = 0;
    p->msg_size = 0;

    mqd_t mqd = mq_open(name, O_RDWR);
    if (mqd < 0) {
        if (errno == ENOSYS || errno == ENOTSUP || errno == ENOENT) {
            return OE_ERR_UNSUPPORTED;
        }
        return OE_ERR_INTERNAL;
    }

    p->mq = mqd;
    p->inited = 1;
    return OE_OK;
}

oe_result_t oe_mq_close(oe_mq_t *mq)
{
    oe_mq_impl_t *p;

    if (!mq) {
        return OE_ERR_INVALID_ARG;
    }
    p = impl(mq);
    if (!p->inited) {
        return OE_OK;
    }
    if (mq_close(p->mq) != 0) {
        /* still clear */
        memset(mq, 0, sizeof(*mq));
        return OE_ERR_INTERNAL;
    }
    memset(mq, 0, sizeof(*mq));
    return OE_OK;
}

oe_result_t oe_mq_unlink(const char *name)
{
    if (!name) {
        return OE_ERR_INVALID_ARG;
    }
    if (mq_unlink(name) != 0) {
        if (errno == ENOENT) {
            return OE_OK;
        }
        return OE_ERR_INTERNAL;
    }
    return OE_OK;
}

oe_result_t oe_mq_send(oe_mq_t *mq,
                         const void *buf,
                         size_t len,
                         int timeout_ms)
{
    oe_mq_impl_t *p;

    if (!mq || (!buf && len > 0)) {
        return OE_ERR_INVALID_ARG;
    }

    p = impl(mq);
    if (!p->inited || p->mq == (mqd_t)-1) {
        return OE_ERR_INVALID_ARG;
    }

    if (timeout_ms < 0) {
        if (mq_send(p->mq, (const char *)buf, len, 0) != 0) {
            if (errno == EAGAIN) {
                return OE_ERR_AGAIN;
            }
            return OE_ERR_INTERNAL;
        }
        return OE_OK;
    }

    struct timespec ts;
    if (abs_deadline_ms(timeout_ms, &ts) != OE_OK) {
        return OE_ERR_INTERNAL;
    }

    if (mq_timedsend(p->mq, (const char *)buf, len, 0, &ts) != 0) {
        if (errno == ETIMEDOUT) {
            return OE_ERR_TIMEOUT;
        }
        if (errno == EAGAIN) {
            return OE_ERR_AGAIN;
        }
        return OE_ERR_INTERNAL;
    }
    return OE_OK;
}

oe_result_t oe_mq_recv(oe_mq_t *mq,
                         void *buf,
                         size_t cap,
                         size_t *out_len,
                         int timeout_ms)
{
    oe_mq_impl_t *p;

    if (!mq || !buf || cap == 0) {
        return OE_ERR_INVALID_ARG;
    }

    p = impl(mq);
    if (!p->inited || p->mq == (mqd_t)-1) {
        return OE_ERR_INVALID_ARG;
    }

    if (out_len) {
        *out_len = 0;
    }

    if (timeout_ms < 0) {
        ssize_t n = mq_receive(p->mq, (char *)buf, cap, NULL);
        if (n < 0) {
            return OE_ERR_INTERNAL;
        }
        if (out_len) {
            *out_len = (size_t)n;
        }
        return OE_OK;
    }

    struct timespec ts;
    if (abs_deadline_ms(timeout_ms, &ts) != OE_OK) {
        return OE_ERR_INTERNAL;
    }

    ssize_t n = mq_timedreceive(p->mq, (char *)buf, cap, NULL, &ts);
    if (n < 0) {
        if (errno == ETIMEDOUT) {
            return OE_ERR_TIMEOUT;
        }
        if (errno == EAGAIN) {
            return OE_ERR_AGAIN;
        }
        return OE_ERR_INTERNAL;
    }

    if (out_len) {
        *out_len = (size_t)n;
    }
    return OE_OK;
}

