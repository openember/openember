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
#include <netdb.h>
#include <unistd.h>

typedef struct {
    int fd;
    int inited;
    int is_server;
    int is_dgram;
    int family;
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
    p->family = AF_UNIX;
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
    p->family = AF_UNIX;

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

static oe_result_t gai_make_port_string(uint16_t port, char out[16])
{
    int n = snprintf(out, 16, "%u", (unsigned)port);
    return (n > 0 && n < 16) ? OE_OK : OE_ERR_INTERNAL;
}

oe_result_t oe_socket_open_tcp_server(oe_socket_t *s, const char *bind_ip, uint16_t port, uint32_t backlog)
{
    oe_socket_impl_t *p;
    int yes = 1;
    struct addrinfo hints;
    struct addrinfo *res = NULL;
    struct addrinfo *ai = NULL;
    char port_str[16];
    int fd = -1;

    if (!s) {
        return OE_ERR_INVALID_ARG;
    }

    memset(s, 0, sizeof(*s));
    p = impl(s);
    p->fd = -1;
    p->inited = 0;
    p->is_server = 1;
    p->is_dgram = 0;
    p->family = AF_UNSPEC;
    p->backlog = backlog;

    if (gai_make_port_string(port, port_str) != OE_OK) {
        return OE_ERR_INTERNAL;
    }

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    if (getaddrinfo((bind_ip && bind_ip[0]) ? bind_ip : NULL, port_str, &hints, &res) != 0) {
        return OE_ERR_INVALID_ARG;
    }

    for (ai = res; ai; ai = ai->ai_next) {
        fd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
        if (fd < 0) {
            continue;
        }
        (void)setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
        if (set_nonblocking(fd, 1) != OE_OK) {
            close(fd);
            fd = -1;
            continue;
        }
        if (bind(fd, ai->ai_addr, (socklen_t)ai->ai_addrlen) != 0) {
            close(fd);
            fd = -1;
            continue;
        }
        if (listen(fd, (int)backlog) != 0) {
            close(fd);
            fd = -1;
            continue;
        }
        p->family = ai->ai_family;
        break;
    }

    freeaddrinfo(res);
    if (fd < 0) {
        return OE_ERR_INTERNAL;
    }

    p->fd = fd;
    p->inited = 1;
    return OE_OK;
}

oe_result_t oe_socket_open_tcp_client(oe_socket_t *s, const char *host_ip, uint16_t port, int timeout_ms)
{
    oe_socket_impl_t *p;
    struct addrinfo hints;
    struct addrinfo *res = NULL;
    struct addrinfo *ai = NULL;
    char port_str[16];
    int fd = -1;

    if (!s || !host_ip) {
        return OE_ERR_INVALID_ARG;
    }

    memset(s, 0, sizeof(*s));
    p = impl(s);
    p->fd = -1;
    p->inited = 0;
    p->is_server = 0;
    p->is_dgram = 0;
    p->family = AF_UNSPEC;

    if (gai_make_port_string(port, port_str) != OE_OK) {
        return OE_ERR_INTERNAL;
    }

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if (getaddrinfo(host_ip, port_str, &hints, &res) != 0) {
        return OE_ERR_INVALID_ARG;
    }

    for (ai = res; ai; ai = ai->ai_next) {
        fd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
        if (fd < 0) {
            continue;
        }
        if (set_nonblocking(fd, 1) != OE_OK) {
            close(fd);
            fd = -1;
            continue;
        }

        for (;;) {
            if (connect(fd, ai->ai_addr, (socklen_t)ai->ai_addrlen) == 0) {
                p->family = ai->ai_family;
                goto connected;
            }
            if (errno == EINPROGRESS) {
                struct pollfd fds[1];
                fds[0].fd = fd;
                fds[0].events = POLLOUT;
                int rc = poll(fds, 1, timeout_ms);
                if (rc == 0) {
                    close(fd);
                    fd = -1;
                    goto next_ai;
                }
                if (rc < 0 && errno != EINTR) {
                    close(fd);
                    fd = -1;
                    goto next_ai;
                }
                int err = 0;
                socklen_t errlen = sizeof(err);
                if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &err, &errlen) != 0 || err != 0) {
                    close(fd);
                    fd = -1;
                    goto next_ai;
                }
                p->family = ai->ai_family;
                goto connected;
            }
            if (errno == EINTR) {
                continue;
            }
            close(fd);
            fd = -1;
            break;
        }
next_ai:
        continue;
    }

connected:
    freeaddrinfo(res);
    if (fd < 0) {
        return OE_ERR_INTERNAL;
    }

    p->fd = fd;
    p->inited = 1;
    return OE_OK;
}

oe_result_t oe_socket_open_udp(oe_socket_t *s, const char *bind_ip, uint16_t port)
{
    oe_socket_impl_t *p;
    struct addrinfo hints;
    struct addrinfo *res = NULL;
    struct addrinfo *ai = NULL;
    char port_str[16];
    int fd = -1;

    if (!s) {
        return OE_ERR_INVALID_ARG;
    }

    memset(s, 0, sizeof(*s));
    p = impl(s);
    p->fd = -1;
    p->inited = 0;
    p->is_server = 0;
    p->is_dgram = 1;
    p->family = AF_UNSPEC;

    if (gai_make_port_string(port, port_str) != OE_OK) {
        return OE_ERR_INTERNAL;
    }

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_PASSIVE;

    if (getaddrinfo((bind_ip && bind_ip[0]) ? bind_ip : NULL, port_str, &hints, &res) != 0) {
        return OE_ERR_INVALID_ARG;
    }

    for (ai = res; ai; ai = ai->ai_next) {
        fd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
        if (fd < 0) {
            continue;
        }
        if (set_nonblocking(fd, 1) != OE_OK) {
            close(fd);
            fd = -1;
            continue;
        }
        if (bind(fd, ai->ai_addr, (socklen_t)ai->ai_addrlen) != 0) {
            close(fd);
            fd = -1;
            continue;
        }
        p->family = ai->ai_family;
        break;
    }

    freeaddrinfo(res);
    if (fd < 0) {
        return OE_ERR_INTERNAL;
    }

    p->fd = fd;
    p->inited = 1;
    return OE_OK;
}

oe_result_t oe_socket_udp_connect(oe_socket_t *s, const char *host_ip, uint16_t port)
{
    oe_socket_impl_t *p;
    struct addrinfo hints;
    struct addrinfo *res = NULL;
    struct addrinfo *ai = NULL;
    char port_str[16];

    if (!s || !host_ip) {
        return OE_ERR_INVALID_ARG;
    }
    p = impl(s);
    if (!p->inited || p->fd < 0 || !p->is_dgram) {
        return OE_ERR_INVALID_ARG;
    }

    if (gai_make_port_string(port, port_str) != OE_OK) {
        return OE_ERR_INTERNAL;
    }
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;
    if (getaddrinfo(host_ip, port_str, &hints, &res) != 0) {
        return OE_ERR_INVALID_ARG;
    }
    for (ai = res; ai; ai = ai->ai_next) {
        if (connect(p->fd, ai->ai_addr, (socklen_t)ai->ai_addrlen) == 0) {
            p->family = ai->ai_family;
            freeaddrinfo(res);
            return OE_OK;
        }
    }
    freeaddrinfo(res);
    return OE_ERR_INTERNAL;
    return OE_OK;
}

oe_result_t oe_socket_get_local_port(oe_socket_t *s, uint16_t *out_port)
{
    oe_socket_impl_t *p;
    struct sockaddr_storage ss;
    socklen_t len = sizeof(ss);

    if (!s || !out_port) {
        return OE_ERR_INVALID_ARG;
    }
    p = impl(s);
    if (!p->inited || p->fd < 0) {
        return OE_ERR_INVALID_ARG;
    }
    memset(&ss, 0, sizeof(ss));
    if (getsockname(p->fd, (struct sockaddr *)&ss, &len) != 0) {
        return OE_ERR_INTERNAL;
    }
    if (((struct sockaddr *)&ss)->sa_family == AF_INET) {
        *out_port = ntohs(((struct sockaddr_in *)&ss)->sin_port);
        return OE_OK;
    }
    if (((struct sockaddr *)&ss)->sa_family == AF_INET6) {
        *out_port = ntohs(((struct sockaddr_in6 *)&ss)->sin6_port);
        return OE_OK;
    }
    return OE_ERR_UNSUPPORTED;
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

static oe_result_t sockaddr_from_sa(oe_sockaddr_t *out, const struct sockaddr *sa, socklen_t slen)
{
    if (!out || !sa) {
        return OE_ERR_INVALID_ARG;
    }
    if (slen > (socklen_t)sizeof(out->storage)) {
        return OE_ERR_NOMEM;
    }
    memset(out, 0, sizeof(*out));
    memcpy(out->storage, sa, (size_t)slen);
    out->len = (uint32_t)slen;
    return OE_OK;
}

oe_result_t oe_socket_sendto(oe_socket_t *s,
                             const void *buf,
                             size_t len,
                             const oe_sockaddr_t *to,
                             size_t *out_sent,
                             int timeout_ms)
{
    oe_socket_impl_t *p;

    if (out_sent) {
        *out_sent = 0;
    }
    if (!s || (!buf && len > 0) || !out_sent || !to || to->len == 0) {
        return OE_ERR_INVALID_ARG;
    }

    p = impl(s);
    if (!p->inited || p->fd < 0 || !p->is_dgram) {
        return OE_ERR_UNSUPPORTED;
    }

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

    ssize_t n = sendto(p->fd, buf, len, 0, (const struct sockaddr *)to->storage, (socklen_t)to->len);
    if (n < 0) {
        if (errno == EINTR) {
            return OE_ERR_AGAIN;
        }
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return OE_ERR_TIMEOUT;
        }
        return OE_ERR_INTERNAL;
    }
    *out_sent = (size_t)n;
    return ((size_t)n == len) ? OE_OK : OE_ERR_INTERNAL;
}

oe_result_t oe_socket_recvfrom(oe_socket_t *s,
                               void *buf,
                               size_t len,
                               size_t *out_received,
                               oe_sockaddr_t *out_from,
                               int timeout_ms)
{
    oe_socket_impl_t *p;
    struct sockaddr_storage ss;
    socklen_t slen = sizeof(ss);

    if (out_received) {
        *out_received = 0;
    }
    if (!s || (!buf && len > 0) || !out_received || !out_from) {
        return OE_ERR_INVALID_ARG;
    }

    p = impl(s);
    if (!p->inited || p->fd < 0 || !p->is_dgram) {
        return OE_ERR_UNSUPPORTED;
    }

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

    memset(&ss, 0, sizeof(ss));
    ssize_t n = recvfrom(p->fd, buf, len, 0, (struct sockaddr *)&ss, &slen);
    if (n < 0) {
        if (errno == EINTR) {
            return OE_ERR_AGAIN;
        }
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return OE_ERR_TIMEOUT;
        }
        return OE_ERR_INTERNAL;
    }
    *out_received = (size_t)n;
    (void)sockaddr_from_sa(out_from, (struct sockaddr *)&ss, slen);
    return OE_OK;
}

