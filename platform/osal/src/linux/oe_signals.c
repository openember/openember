#define _GNU_SOURCE

#include "openember/osal/signals.h"

#include <errno.h>
#include <poll.h>
#include <signal.h>
#include <string.h>
#include <sys/signalfd.h>
#include <unistd.h>
#include <signal.h>

typedef struct {
    int fd;
    int inited;
    sigset_t oldmask;
    int has_oldmask;
} oe_signals_impl_t;

_Static_assert(sizeof(oe_signals_impl_t) <= sizeof(oe_signals_t), "oe_signals_t opaque too small (signals)");

static oe_signals_impl_t *impl(oe_signals_t *s)
{
    return (oe_signals_impl_t *)(void *)s->opaque;
}

oe_result_t oe_signals_query_caps(oe_signals_caps_t *out_caps)
{
    if (!out_caps) {
        return OE_ERR_INVALID_ARG;
    }
    out_caps->supports_signalfd = 1u;
    return OE_OK;
}

oe_result_t oe_signals_open(oe_signals_t *s, const int *signums, size_t count)
{
    sigset_t mask;
    oe_signals_impl_t *p;

    if (!s || !signums || count == 0) {
        return OE_ERR_INVALID_ARG;
    }

    memset(s, 0, sizeof(*s));
    p = impl(s);
    p->fd = -1;
    p->inited = 0;
    p->has_oldmask = 0;

    sigemptyset(&mask);
    for (size_t i = 0; i < count; i++) {
        if (signums[i] <= 0) {
            return OE_ERR_INVALID_ARG;
        }
        sigaddset(&mask, (int)signums[i]);
    }

    /* Block signals process-wide and remember previous mask.
     * This avoids other threads receiving the signal with default handlers.
     */
    if (sigprocmask(SIG_BLOCK, &mask, &p->oldmask) != 0) {
        return OE_ERR_INTERNAL;
    }
    p->has_oldmask = 1;

    int flags = SFD_NONBLOCK | SFD_CLOEXEC;
    p->fd = signalfd(-1, &mask, flags);
    if (p->fd < 0) {
        if (p->has_oldmask) {
            (void)sigprocmask(SIG_SETMASK, &p->oldmask, NULL);
        }
        return OE_ERR_INTERNAL;
    }

    p->inited = 1;
    return OE_OK;
}

oe_result_t oe_signals_wait(oe_signals_t *s, oe_signal_info_t *out_info, int timeout_ms)
{
    oe_signals_impl_t *p;
    struct pollfd fds[1];
    int rc;

    if (!s || !out_info) {
        return OE_ERR_INVALID_ARG;
    }

    p = impl(s);
    if (!p->inited || p->fd < 0) {
        return OE_ERR_INVALID_ARG;
    }

    fds[0].fd = p->fd;
    fds[0].events = POLLIN;
    fds[0].revents = 0;

    rc = poll(fds, 1, timeout_ms);
    if (rc < 0) {
        if (errno == EINTR) {
            return OE_ERR_AGAIN;
        }
        return OE_ERR_INTERNAL;
    }
    if (rc == 0) {
        return OE_ERR_TIMEOUT;
    }

    struct signalfd_siginfo info;
    memset(&info, 0, sizeof(info));
    ssize_t n = read(p->fd, &info, sizeof(info));
    if (n != (ssize_t)sizeof(info)) {
        if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            return OE_ERR_TIMEOUT;
        }
        return OE_ERR_INTERNAL;
    }

    out_info->signum = (int)info.ssi_signo;
    out_info->sender_pid = (int32_t)info.ssi_pid;
    return OE_OK;
}

oe_result_t oe_signals_close(oe_signals_t *s)
{
    oe_signals_impl_t *p;
    int fd;
    int has_oldmask;
    sigset_t oldmask;

    if (!s) {
        return OE_ERR_INVALID_ARG;
    }

    p = impl(s);
    fd = p->fd;
    has_oldmask = p->has_oldmask;
    oldmask = p->oldmask;

    memset(s, 0, sizeof(*s));

    if (fd >= 0) {
        (void)close(fd);
    }
    if (has_oldmask) {
        (void)sigprocmask(SIG_SETMASK, &oldmask, NULL);
    }
    return OE_OK;
}

