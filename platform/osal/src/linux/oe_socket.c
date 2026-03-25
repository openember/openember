#define _GNU_SOURCE

#include "openember/osal/socket.h"

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>

typedef struct {
    int fd;
    int inited;
    int is_server;
    int is_dgram;
    char path[sizeof(((struct sockaddr_un *)0)->sun_path)];
    uint32_t backlog;
} oe_socket_impl_t;

_Static_assert(sizeof(oe_socket_impl_t) <= sizeof(oe_socket_t), "oe_socket_t opaque too small (socket)");

static oe_socket_impl_t *impl(oe_socket_t *s)
{
    return (oe_socket_impl_t *)(void *)s->opaque;
}

static oe_result_t set_nonblocking(int fd, int enable)
{
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) {
        return OE_ERR_INTERNAL;
    }
    if (enable) {
        flags |= O_NONBLOCK;
    } else {
        flags &= ~O_NONBLOCK;
    }
    if (fcntl(fd, F_SETFL, flags) != 0) {
        return OE_ERR_INTERNAL;
    }
    return OE_OK;
}

oe_result_t oe_socket_query_caps(oe_socket_caps_t *out_caps)
{
    if (!out_caps) {
        return OE_ERR_INVALID_ARG;
    }
    out_caps->supports_unix_domain = 1u;
    out_caps->supports_tcp = 1u;
    out_caps->supports_udp = 1u;
    return OE_OK;
}

oe_result_t oe_socket_open_unix_server(oe_socket_t *s, const char *path, uint32_t backlog)
{
    oe_socket_impl_t *p;
    int fd;
    struct sockaddr_un addr;
    size_t path_len;

    if (!s || !path) {
        return OE_ERR_INVALID_ARG;
    }

    memset(s, 0, sizeof(*s));
    p = impl(s);
    p->fd = -1;
    p->inited = 0;
    p->is_server = 1;
    p->is_dgram = 0;
    p->backlog = backlog;

    fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) {
        return OE_ERR_INTERNAL;
    }

    if (set_nonblocking(fd, 1) != OE_OK) {
        close(fd);
        return OE_ERR_INTERNAL;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    path_len = strnlen(path, sizeof(addr.sun_path) - 1);
    if (path_len == 0) {
        close(fd);
        return OE_ERR_INVALID_ARG;
    }
    memcpy(addr.sun_path, path, path_len);
    addr.sun_path[path_len] = '\0';

    /* Remove existing endpoint (Linux allows bind over old file only after unlink). */
    (void)unlink(addr.sun_path);

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
        close(fd);
        return OE_ERR_INTERNAL;
    }

    if (listen(fd, (int)backlog) != 0) {
        close(fd);
        return OE_ERR_INTERNAL;
    }

    p->fd = fd;
    p->inited = 1;
    strncpy(p->path, addr.sun_path, sizeof(p->path) - 1);
    p->path[sizeof(p->path) - 1] = '\0';
    return OE_OK;
}

oe_result_t oe_socket_open_unix_client(oe_socket_t *s, const char *path)
{
    oe_socket_impl_t *p;
    int fd;
    struct sockaddr_un addr;
    size_t path_len;

    if (!s || !path) {
        return OE_ERR_INVALID_ARG;
    }

    memset(s, 0, sizeof(*s));
    p = impl(s);
    p->fd = -1;
    p->inited = 0;
    p->is_server = 0;
    p->is_dgram = 0;

    fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) {
        return OE_ERR_INTERNAL;
    }

    if (set_nonblocking(fd, 1) != OE_OK) {
        close(fd);
        return OE_ERR_INTERNAL;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    path_len = strnlen(path, sizeof(addr.sun_path) - 1);
    if (path_len == 0) {
        close(fd);
        return OE_ERR_INVALID_ARG;
    }
    memcpy(addr.sun_path, path, path_len);
    addr.sun_path[path_len] = '\0';

    for (;;) {
        if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) == 0) {
            break;
        }
        if (errno == EINPROGRESS) {
            struct pollfd fds[1];
            fds[0].fd = fd;
            fds[0].events = POLLOUT;
            int rc = poll(fds, 1, -1);
            if (rc < 0 && errno != EINTR) {
                close(fd);
                return OE_ERR_INTERNAL;
            }
            continue;
        }
        close(fd);
        return OE_ERR_INTERNAL;
    }

    p->fd = fd;
    p->inited = 1;
    return OE_OK;
}

static oe_result_t sockaddr_in_from_ip_port(const char *ip, uint16_t port, struct sockaddr_in *out)
{
    if (!out) {
        return OE_ERR_INVALID_ARG;
    }
    memset(out, 0, sizeof(*out));
    out->sin_family = AF_INET;
    out->sin_port = htons(port);
    if (!ip || ip[0] == '\0') {
        out->sin_addr.s_addr = htonl(INADDR_ANY);
        return OE_OK;
    }
    if (inet_pton(AF_INET, ip, &out->sin_addr) != 1) {
        return OE_ERR_INVALID_ARG;
    }
    return OE_OK;
}

oe_result_t oe_socket_open_tcp_server(oe_socket_t *s, const char *bind_ip, uint16_t port, uint32_t backlog)
{
    oe_socket_impl_t *p;
    int fd;
    struct sockaddr_in addr;
    int yes = 1;
    oe_result_t r;

    if (!s) {
        return OE_ERR_INVALID_ARG;
    }

    memset(s, 0, sizeof(*s));
    p = impl(s);
    p->fd = -1;
    p->inited = 0;
    p->is_server = 1;
    p->is_dgram = 0;
    p->backlog = backlog;

    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        return OE_ERR_INTERNAL;
    }
    (void)setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
    if (set_nonblocking(fd, 1) != OE_OK) {
        close(fd);
        return OE_ERR_INTERNAL;
    }

    r = sockaddr_in_from_ip_port(bind_ip, port, &addr);
    if (r != OE_OK) {
        close(fd);
        return r;
    }
    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
        close(fd);
        return OE_ERR_INTERNAL;
    }
    if (listen(fd, (int)backlog) != 0) {
        close(fd);
        return OE_ERR_INTERNAL;
    }

    p->fd = fd;
    p->inited = 1;
    return OE_OK;
}

oe_result_t oe_socket_open_tcp_client(oe_socket_t *s, const char *host_ip, uint16_t port, int timeout_ms)
{
    oe_socket_impl_t *p;
    int fd;
    struct sockaddr_in addr;
    oe_result_t r;

    if (!s || !host_ip) {
        return OE_ERR_INVALID_ARG;
    }

    memset(s, 0, sizeof(*s));
    p = impl(s);
    p->fd = -1;
    p->inited = 0;
    p->is_server = 0;
    p->is_dgram = 0;

    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        return OE_ERR_INTERNAL;
    }
    if (set_nonblocking(fd, 1) != OE_OK) {
        close(fd);
        return OE_ERR_INTERNAL;
    }

    r = sockaddr_in_from_ip_port(host_ip, port, &addr);
    if (r != OE_OK) {
        close(fd);
        return r;
    }

    for (;;) {
        if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) == 0) {
            break;
        }
        if (errno == EINPROGRESS) {
            struct pollfd fds[1];
            fds[0].fd = fd;
            fds[0].events = POLLOUT;
            int rc = poll(fds, 1, timeout_ms);
            if (rc == 0) {
                close(fd);
                return OE_ERR_TIMEOUT;
            }
            if (rc < 0 && errno != EINTR) {
                close(fd);
                return OE_ERR_INTERNAL;
            }
            int err = 0;
            socklen_t errlen = sizeof(err);
            if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &err, &errlen) != 0 || err != 0) {
                close(fd);
                return OE_ERR_INTERNAL;
            }
            break;
        }
        if (errno == EINTR) {
            continue;
        }
        close(fd);
        return OE_ERR_INTERNAL;
    }

    p->fd = fd;
    p->inited = 1;
    return OE_OK;
}

oe_result_t oe_socket_open_udp(oe_socket_t *s, const char *bind_ip, uint16_t port)
{
    oe_socket_impl_t *p;
    int fd;
    struct sockaddr_in addr;
    oe_result_t r;

    if (!s) {
        return OE_ERR_INVALID_ARG;
    }

    memset(s, 0, sizeof(*s));
    p = impl(s);
    p->fd = -1;
    p->inited = 0;
    p->is_server = 0;
    p->is_dgram = 1;

    fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
        return OE_ERR_INTERNAL;
    }
    if (set_nonblocking(fd, 1) != OE_OK) {
        close(fd);
        return OE_ERR_INTERNAL;
    }

    r = sockaddr_in_from_ip_port(bind_ip, port, &addr);
    if (r != OE_OK) {
        close(fd);
        return r;
    }
    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
        close(fd);
        return OE_ERR_INTERNAL;
    }

    p->fd = fd;
    p->inited = 1;
    return OE_OK;
}

oe_result_t oe_socket_udp_connect(oe_socket_t *s, const char *host_ip, uint16_t port)
{
    oe_socket_impl_t *p;
    struct sockaddr_in addr;
    oe_result_t r;

    if (!s || !host_ip) {
        return OE_ERR_INVALID_ARG;
    }
    p = impl(s);
    if (!p->inited || p->fd < 0 || !p->is_dgram) {
        return OE_ERR_INVALID_ARG;
    }

    r = sockaddr_in_from_ip_port(host_ip, port, &addr);
    if (r != OE_OK) {
        return r;
    }
    if (connect(p->fd, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
        return OE_ERR_INTERNAL;
    }
    return OE_OK;
}

oe_result_t oe_socket_get_local_port(oe_socket_t *s, uint16_t *out_port)
{
    oe_socket_impl_t *p;
    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);

    if (!s || !out_port) {
        return OE_ERR_INVALID_ARG;
    }
    p = impl(s);
    if (!p->inited || p->fd < 0) {
        return OE_ERR_INVALID_ARG;
    }
    memset(&addr, 0, sizeof(addr));
    if (getsockname(p->fd, (struct sockaddr *)&addr, &len) != 0) {
        return OE_ERR_INTERNAL;
    }
    if (addr.sin_family != AF_INET) {
        return OE_ERR_UNSUPPORTED;
    }
    *out_port = ntohs(addr.sin_port);
    return OE_OK;
}

oe_result_t oe_socket_close(oe_socket_t *s)
{
    oe_socket_impl_t *p;
    int fd;
    int is_server;
    char path_copy[sizeof(((struct sockaddr_un *)0)->sun_path)];

    if (!s) {
        return OE_ERR_INVALID_ARG;
    }

    p = impl(s);
    fd = p->fd;
    is_server = p->is_server;
    memset(path_copy, 0, sizeof(path_copy));
    strncpy(path_copy, p->path, sizeof(path_copy) - 1);
    memset(s, 0, sizeof(*s));

    if (fd >= 0) {
        (void)close(fd);
    }

    /* For server sockets, unlink endpoint. */
    if (is_server && path_copy[0] != '\0') {
        (void)unlink(path_copy);
    }
    return OE_OK;
}

oe_result_t oe_socket_accept(oe_socket_t *server, oe_socket_t *client, int timeout_ms)
{
    oe_socket_impl_t *sp;
    oe_socket_impl_t *cp;
    struct pollfd fds[1];
    int rc;

    if (!server || !client) {
        return OE_ERR_INVALID_ARG;
    }
    sp = impl(server);
    if (!sp->inited || sp->fd < 0) {
        return OE_ERR_INVALID_ARG;
    }
    if (sp->is_dgram) {
        return OE_ERR_UNSUPPORTED;
    }

    fds[0].fd = sp->fd;
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

    /* accept into client fd */
    memset(client, 0, sizeof(*client));
    cp = impl(client);
    cp->fd = -1;
    cp->inited = 0;

    for (;;) {
        int newfd = accept(sp->fd, NULL, NULL);
        if (newfd >= 0) {
            if (set_nonblocking(newfd, 1) != OE_OK) {
                close(newfd);
                return OE_ERR_INTERNAL;
            }
            cp->fd = newfd;
            cp->inited = 1;
            cp->is_server = 0;
            return OE_OK;
        }
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return OE_ERR_TIMEOUT;
        }
        if (errno == EINTR) {
            continue;
        }
        return OE_ERR_INTERNAL;
    }
}

oe_result_t oe_socket_send(oe_socket_t *s,
                            const void *buf,
                            size_t len,
                            size_t *out_sent,
                            int timeout_ms)
{
    oe_socket_impl_t *p;
    const uint8_t *ptr = (const uint8_t *)buf;
    size_t total = 0;

    if (!s || (!buf && len > 0)) {
        return OE_ERR_INVALID_ARG;
    }

    p = impl(s);
    if (!p->inited || p->fd < 0) {
        return OE_ERR_INVALID_ARG;
    }

    if (out_sent) {
        *out_sent = 0;
    }

    /* Datagram: single send attempt */
    if (p->is_dgram) {
        struct pollfd fds[1];
        fds[0].fd = p->fd;
        fds[0].events = POLLOUT;
        fds[0].revents = 0;
        int rc = poll(fds, 1, timeout_ms);
        if (rc == 0) {
            return OE_ERR_TIMEOUT;
        }
        if (rc < 0) {
            if (errno == EINTR) {
                return OE_ERR_AGAIN;
            }
            return OE_ERR_INTERNAL;
        }
        ssize_t n = send(p->fd, ptr, len, 0);
        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                return OE_ERR_TIMEOUT;
            }
            return OE_ERR_INTERNAL;
        }
        total = (size_t)n;
        if (out_sent) {
            *out_sent = total;
        }
        return (total == len) ? OE_OK : OE_ERR_INTERNAL;
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
            return (timeout_ms >= 0) ? OE_ERR_TIMEOUT : OE_ERR_INTERNAL;
        }

        ssize_t n = send(p->fd, ptr + total, len - total, 0);
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

    if (out_sent) {
        *out_sent = total;
    }
    return OE_OK;
}

oe_result_t oe_socket_recv(oe_socket_t *s,
                            void *buf,
                            size_t len,
                            size_t *out_received,
                            int timeout_ms)
{
    oe_socket_impl_t *p;
    uint8_t *ptr = (uint8_t *)buf;
    size_t total = 0;

    if (!s || (!buf && len > 0)) {
        return OE_ERR_INVALID_ARG;
    }

    p = impl(s);
    if (!p->inited || p->fd < 0) {
        return OE_ERR_INVALID_ARG;
    }

    if (out_received) {
        *out_received = 0;
    }

    /* Datagram: single recv attempt (up to len) */
    if (p->is_dgram) {
        struct pollfd fds[1];
        fds[0].fd = p->fd;
        fds[0].events = POLLIN;
        fds[0].revents = 0;
        int rc = poll(fds, 1, timeout_ms);
        if (rc == 0) {
            return OE_ERR_TIMEOUT;
        }
        if (rc < 0) {
            if (errno == EINTR) {
                return OE_ERR_AGAIN;
            }
            return OE_ERR_INTERNAL;
        }
        ssize_t n = recv(p->fd, ptr, len, 0);
        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                return OE_ERR_TIMEOUT;
            }
            return OE_ERR_INTERNAL;
        }
        if (n == 0) {
            return OE_ERR_INTERNAL;
        }
        total = (size_t)n;
        if (out_received) {
            *out_received = total;
        }
        return OE_OK;
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

        ssize_t n = recv(p->fd, ptr + total, len - total, 0);
        if (n > 0) {
            total += (size_t)n;
            continue;
        }
        if (n == 0) {
            /* peer closed */
            return OE_ERR_INTERNAL;
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

    if (out_received) {
        *out_received = total;
    }
    return OE_OK;
}

