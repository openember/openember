#define _GNU_SOURCE

#include "openember/osal/fifo.h"

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

typedef struct {
    int fd;
    int inited;
    uint32_t mode;
    char path[256];
} oe_fifo_impl_t;

_Static_assert(sizeof(oe_fifo_impl_t) <= sizeof(oe_fifo_t), "oe_fifo_t opaque too small (fifo)");

static oe_fifo_impl_t *impl(oe_fifo_t *f)
{
    return (oe_fifo_impl_t *)(void *)f->opaque;
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

oe_result_t oe_fifo_query_caps(oe_fifo_caps_t *out_caps)
{
    if (!out_caps) {
        return OE_ERR_INVALID_ARG;
    }
    out_caps->supports_fifo = 1u;
    return OE_OK;
}

oe_result_t oe_fifo_open(oe_fifo_t *f, const char *path, uint32_t mode)
{
    oe_fifo_impl_t *p;
    struct stat st;
    int fd;

    if (!f || !path) {
        return OE_ERR_INVALID_ARG;
    }

    memset(f, 0, sizeof(*f));
    p = impl(f);
    p->fd = -1;
    p->inited = 0;
    p->mode = mode;
    strncpy(p->path, path, sizeof(p->path) - 1);
    p->path[sizeof(p->path) - 1] = '\0';

    if ((mode & (OE_FIFO_MODE_READ | OE_FIFO_MODE_WRITE)) == 0) {
        return OE_ERR_INVALID_ARG;
    }

    /* Create FIFO if missing */
    if (stat(path, &st) != 0) {
        if (errno == ENOENT) {
            if (mkfifo(path, 0666) != 0) {
                return OE_ERR_INTERNAL;
            }
        } else {
            return OE_ERR_INTERNAL;
        }
    }

    /* Open with O_RDWR to avoid open-order blocking with separate readers/writers. */
    fd = open(path, O_RDWR | O_NONBLOCK);
    if (fd < 0) {
        return OE_ERR_INTERNAL;
    }

    if (set_nonblocking(fd) != OE_OK) {
        close(fd);
        return OE_ERR_INTERNAL;
    }

    p->fd = fd;
    p->inited = 1;
    return OE_OK;
}

oe_result_t oe_fifo_close(oe_fifo_t *f)
{
    oe_fifo_impl_t *p;
    int fd;

    if (!f) {
        return OE_ERR_INVALID_ARG;
    }

    p = impl(f);
    fd = p->fd;
    memset(f, 0, sizeof(*f));
    if (fd >= 0) {
        (void)close(fd);
    }
    return OE_OK;
}

oe_result_t oe_fifo_unlink(const char *path)
{
    if (!path) {
        return OE_ERR_INVALID_ARG;
    }
    if (unlink(path) != 0) {
        if (errno == ENOENT) {
            return OE_OK;
        }
        return OE_ERR_INTERNAL;
    }
    return OE_OK;
}

oe_result_t oe_fifo_write(oe_fifo_t *f,
                            const void *buf,
                            size_t len,
                            size_t *out_written,
                            int timeout_ms)
{
    oe_fifo_impl_t *p;
    const uint8_t *ptr = (const uint8_t *)buf;
    size_t total = 0;

    if (!f || (!buf && len > 0)) {
        return OE_ERR_INVALID_ARG;
    }
    p = impl(f);
    if (!p->inited || p->fd < 0) {
        return OE_ERR_INVALID_ARG;
    }
    if ((p->mode & OE_FIFO_MODE_WRITE) == 0) {
        return OE_ERR_UNSUPPORTED;
    }

    if (out_written) {
        *out_written = 0;
    }

    for (;;) {
        if (total == len) {
            break;
        }

        struct pollfd fds[1];
        fds[0].fd = p->fd;
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

        ssize_t n = write(p->fd, ptr + total, len - total);
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

oe_result_t oe_fifo_read(oe_fifo_t *f,
                           void *buf,
                           size_t len,
                           size_t *out_read,
                           int timeout_ms)
{
    oe_fifo_impl_t *p;
    uint8_t *ptr = (uint8_t *)buf;
    size_t total = 0;

    if (!f || (!buf && len > 0)) {
        return OE_ERR_INVALID_ARG;
    }
    p = impl(f);
    if (!p->inited || p->fd < 0) {
        return OE_ERR_INVALID_ARG;
    }
    if ((p->mode & OE_FIFO_MODE_READ) == 0) {
        return OE_ERR_UNSUPPORTED;
    }

    if (out_read) {
        *out_read = 0;
    }

    for (;;) {
        if (total == len) {
            break;
        }

        struct pollfd fds[1];
        fds[0].fd = p->fd;
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

        ssize_t n = read(p->fd, ptr + total, len - total);
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

