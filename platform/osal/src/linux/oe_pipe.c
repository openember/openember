#define _GNU_SOURCE

#include "openember/osal/pipe.h"

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

typedef struct {
    int fds[2];
    int inited;
} oe_pipe_impl_t;

_Static_assert(sizeof(oe_pipe_impl_t) <= sizeof(oe_pipe_t), "oe_pipe_t opaque too small (pipe)");

static oe_pipe_impl_t *impl(oe_pipe_t *p)
{
    return (oe_pipe_impl_t *)(void *)p->opaque;
}

static oe_result_t set_nonblocking(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) {
        return OE_ERR_INTERNAL;
    }
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) != 0) {
        return OE_ERR_INTERNAL;
    }
    return OE_OK;
}

oe_result_t oe_pipe_query_caps(oe_pipe_caps_t *out_caps)
{
    if (!out_caps) {
        return OE_ERR_INVALID_ARG;
    }
    out_caps->supports_pipe = 1u;
    return OE_OK;
}

oe_result_t oe_pipe_create(oe_pipe_t *p)
{
    oe_pipe_impl_t *pp;
    int fds[2];

    if (!p) {
        return OE_ERR_INVALID_ARG;
    }

    memset(p, 0, sizeof(*p));
    pp = impl(p);
    pp->fds[0] = -1;
    pp->fds[1] = -1;
    pp->inited = 0;

    if (pipe(fds) != 0) {
        return OE_ERR_INTERNAL;
    }

    if (set_nonblocking(fds[0]) != OE_OK || set_nonblocking(fds[1]) != OE_OK) {
        close(fds[0]);
        close(fds[1]);
        return OE_ERR_INTERNAL;
    }

    pp->fds[0] = fds[0];
    pp->fds[1] = fds[1];
    pp->inited = 1;
    return OE_OK;
}

oe_result_t oe_pipe_close(oe_pipe_t *p)
{
    oe_pipe_impl_t *pp;
    int r0, r1;

    if (!p) {
        return OE_ERR_INVALID_ARG;
    }

    pp = impl(p);
    r0 = pp->fds[0];
    r1 = pp->fds[1];
    memset(p, 0, sizeof(*p));

    if (r0 >= 0) {
        (void)close(r0);
    }
    if (r1 >= 0) {
        (void)close(r1);
    }
    return OE_OK;
}

oe_result_t oe_pipe_write(oe_pipe_t *p,
                            const void *buf,
                            size_t len,
                            size_t *out_written,
                            int timeout_ms)
{
    oe_pipe_impl_t *pp;
    const uint8_t *ptr = (const uint8_t *)buf;
    size_t total = 0;

    if (!p || (!buf && len > 0)) {
        return OE_ERR_INVALID_ARG;
    }

    pp = impl(p);
    if (!pp->inited) {
        return OE_ERR_INVALID_ARG;
    }

    if (out_written) {
        *out_written = 0;
    }

    for (;;) {
        if (total == len) {
            break;
        }

        struct pollfd fds[1];
        fds[0].fd = pp->fds[1];
        fds[0].events = POLLOUT;
        fds[0].revents = 0;

        int rc = poll(fds, 1, timeout_ms);
        if (rc < 0) {
            if (errno == EINTR) {
                return OE_ERR_AGAIN;
            }
            return OE_ERR_INTERNAL;
        }
        if (rc == 0) {
            return OE_ERR_TIMEOUT;
        }

        ssize_t n = write(pp->fds[1], ptr + total, len - total);
        if (n > 0) {
            total += (size_t)n;
            continue;
        }
        if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            if (timeout_ms == 0) {
                return OE_ERR_TIMEOUT;
            }
            continue;
        }
        if (n < 0 && errno == EINTR) {
            continue;
        }
        return OE_ERR_INTERNAL;
    }

    if (out_written) {
        *out_written = total;
    }
    return OE_OK;
}

oe_result_t oe_pipe_read(oe_pipe_t *p,
                           void *buf,
                           size_t len,
                           size_t *out_read,
                           int timeout_ms)
{
    oe_pipe_impl_t *pp;
    uint8_t *ptr = (uint8_t *)buf;
    size_t total = 0;

    if (!p || (!buf && len > 0)) {
        return OE_ERR_INVALID_ARG;
    }

    pp = impl(p);
    if (!pp->inited) {
        return OE_ERR_INVALID_ARG;
    }

    if (out_read) {
        *out_read = 0;
    }

    for (;;) {
        if (total == len) {
            break;
        }

        struct pollfd fds[1];
        fds[0].fd = pp->fds[0];
        fds[0].events = POLLIN;
        fds[0].revents = 0;

        int rc = poll(fds, 1, timeout_ms);
        if (rc < 0) {
            if (errno == EINTR) {
                return OE_ERR_AGAIN;
            }
            return OE_ERR_INTERNAL;
        }
        if (rc == 0) {
            return OE_ERR_TIMEOUT;
        }

        ssize_t n = read(pp->fds[0], ptr + total, len - total);
        if (n > 0) {
            total += (size_t)n;
            continue;
        }
        if (n == 0) {
            return OE_ERR_INTERNAL; /* unexpected EOF */
        }
        if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            if (timeout_ms == 0) {
                return OE_ERR_TIMEOUT;
            }
            continue;
        }
        if (n < 0 && errno == EINTR) {
            continue;
        }
        return OE_ERR_INTERNAL;
    }

    if (out_read) {
        *out_read = total;
    }
    return OE_OK;
}

