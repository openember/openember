#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200809L

#include "openember/osal/detail/socket_ops.hpp"

#include <arpa/inet.h>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "openember/osal/types.hpp"

namespace openember::osal::detail {

#define _GNU_SOURCE


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


static Result set_nonblocking(int fd, int enable)
{
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) {
        return kErrInternal;
    }
    if (enable) {
        flags |= O_NONBLOCK;
    } else {
        flags &= ~O_NONBLOCK;
    }
    if (fcntl(fd, F_SETFL, flags) != 0) {
        return kErrInternal;
    }
    return kOk;
}

Result query_caps(SocketCaps *out_caps)
{
    if (!out_caps) {
        return kErrInvalidArg;
    }
    out_caps->supports_unix_domain = 1u;
    out_caps->supports_tcp = 1u;
    out_caps->supports_udp = 1u;
    return kOk;
}

Result open_unix_server(SocketImpl *s, const char *path, uint32_t backlog)
{
    SocketImpl *p;
    int fd;
    struct sockaddr_un addr;
    size_t path_len;

    if (!s || !path) {
        return kErrInvalidArg;
    }

    memset(s, 0, sizeof(*s));
    p = s;
    p->fd = -1;
    p->inited = 0;
    p->is_server = 1;
    p->is_dgram = 0;
    p->family = AF_UNIX;
    p->backlog = backlog;

    fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) {
        return kErrInternal;
    }

    if (set_nonblocking(fd, 1) != kOk) {
        ::close(fd);
        return kErrInternal;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    path_len = strnlen(path, sizeof(addr.sun_path) - 1);
    if (path_len == 0) {
        ::close(fd);
        return kErrInvalidArg;
    }
    memcpy(addr.sun_path, path, path_len);
    addr.sun_path[path_len] = '\0';

    /* Remove existing endpoint (Linux allows bind over old file only after unlink). */
    (void)unlink(addr.sun_path);

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
        ::close(fd);
        return kErrInternal;
    }

    if (listen(fd, (int)backlog) != 0) {
        ::close(fd);
        return kErrInternal;
    }

    p->fd = fd;
    p->inited = 1;
    strncpy(p->path, addr.sun_path, sizeof(p->path) - 1);
    p->path[sizeof(p->path) - 1] = '\0';
    return kOk;
}

Result open_unix_client(SocketImpl *s, const char *path)
{
    SocketImpl *p;
    int fd;
    struct sockaddr_un addr;
    size_t path_len;

    if (!s || !path) {
        return kErrInvalidArg;
    }

    memset(s, 0, sizeof(*s));
    p = s;
    p->fd = -1;
    p->inited = 0;
    p->is_server = 0;
    p->is_dgram = 0;
    p->family = AF_UNIX;

    fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) {
        return kErrInternal;
    }

    if (set_nonblocking(fd, 1) != kOk) {
        ::close(fd);
        return kErrInternal;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    path_len = strnlen(path, sizeof(addr.sun_path) - 1);
    if (path_len == 0) {
        ::close(fd);
        return kErrInvalidArg;
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
                ::close(fd);
                return kErrInternal;
            }
            continue;
        }
        ::close(fd);
        return kErrInternal;
    }

    p->fd = fd;
    p->inited = 1;
    return kOk;
}

static Result gai_make_port_string(uint16_t port, char out[16])
{
    int n = snprintf(out, 16, "%u", (unsigned)port);
    return (n > 0 && n < 16) ? kOk : kErrInternal;
}

Result open_tcp_server(SocketImpl *s, const char *bind_ip, uint16_t port, uint32_t backlog)
{
    SocketImpl *p;
    int yes = 1;
    struct addrinfo hints;
    struct addrinfo *res = NULL;
    struct addrinfo *ai = NULL;
    char port_str[16];
    int fd = -1;

    if (!s) {
        return kErrInvalidArg;
    }

    memset(s, 0, sizeof(*s));
    p = s;
    p->fd = -1;
    p->inited = 0;
    p->is_server = 1;
    p->is_dgram = 0;
    p->family = AF_UNSPEC;
    p->backlog = backlog;

    if (gai_make_port_string(port, port_str) != kOk) {
        return kErrInternal;
    }

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    if (getaddrinfo((bind_ip && bind_ip[0]) ? bind_ip : NULL, port_str, &hints, &res) != 0) {
        return kErrInvalidArg;
    }

    for (ai = res; ai; ai = ai->ai_next) {
        fd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
        if (fd < 0) {
            continue;
        }
        (void)setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
        if (set_nonblocking(fd, 1) != kOk) {
            ::close(fd);
            fd = -1;
            continue;
        }
        if (bind(fd, ai->ai_addr, (socklen_t)ai->ai_addrlen) != 0) {
            ::close(fd);
            fd = -1;
            continue;
        }
        if (listen(fd, (int)backlog) != 0) {
            ::close(fd);
            fd = -1;
            continue;
        }
        p->family = ai->ai_family;
        break;
    }

    freeaddrinfo(res);
    if (fd < 0) {
        return kErrInternal;
    }

    p->fd = fd;
    p->inited = 1;
    return kOk;
}

Result open_tcp_client(SocketImpl *s, const char *host_ip, uint16_t port, int timeout_ms)
{
    SocketImpl *p;
    struct addrinfo hints;
    struct addrinfo *res = NULL;
    struct addrinfo *ai = NULL;
    char port_str[16];
    int fd = -1;

    if (!s || !host_ip) {
        return kErrInvalidArg;
    }

    memset(s, 0, sizeof(*s));
    p = s;
    p->fd = -1;
    p->inited = 0;
    p->is_server = 0;
    p->is_dgram = 0;
    p->family = AF_UNSPEC;

    if (gai_make_port_string(port, port_str) != kOk) {
        return kErrInternal;
    }

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if (getaddrinfo(host_ip, port_str, &hints, &res) != 0) {
        return kErrInvalidArg;
    }

    for (ai = res; ai; ai = ai->ai_next) {
        fd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
        if (fd < 0) {
            continue;
        }
        if (set_nonblocking(fd, 1) != kOk) {
            ::close(fd);
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
                    ::close(fd);
                    fd = -1;
                    goto next_ai;
                }
                if (rc < 0 && errno != EINTR) {
                    ::close(fd);
                    fd = -1;
                    goto next_ai;
                }
                int err = 0;
                socklen_t errlen = sizeof(err);
                if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &err, &errlen) != 0 || err != 0) {
                    ::close(fd);
                    fd = -1;
                    goto next_ai;
                }
                p->family = ai->ai_family;
                goto connected;
            }
            if (errno == EINTR) {
                continue;
            }
            ::close(fd);
            fd = -1;
            break;
        }
next_ai:
        continue;
    }

connected:
    freeaddrinfo(res);
    if (fd < 0) {
        return kErrInternal;
    }

    p->fd = fd;
    p->inited = 1;
    return kOk;
}

Result open_udp(SocketImpl *s, const char *bind_ip, uint16_t port)
{
    SocketImpl *p;
    struct addrinfo hints;
    struct addrinfo *res = NULL;
    struct addrinfo *ai = NULL;
    char port_str[16];
    int fd = -1;

    if (!s) {
        return kErrInvalidArg;
    }

    memset(s, 0, sizeof(*s));
    p = s;
    p->fd = -1;
    p->inited = 0;
    p->is_server = 0;
    p->is_dgram = 1;
    p->family = AF_UNSPEC;

    if (gai_make_port_string(port, port_str) != kOk) {
        return kErrInternal;
    }

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_PASSIVE;

    if (getaddrinfo((bind_ip && bind_ip[0]) ? bind_ip : NULL, port_str, &hints, &res) != 0) {
        return kErrInvalidArg;
    }

    for (ai = res; ai; ai = ai->ai_next) {
        fd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
        if (fd < 0) {
            continue;
        }
        if (set_nonblocking(fd, 1) != kOk) {
            ::close(fd);
            fd = -1;
            continue;
        }
        if (bind(fd, ai->ai_addr, (socklen_t)ai->ai_addrlen) != 0) {
            ::close(fd);
            fd = -1;
            continue;
        }
        p->family = ai->ai_family;
        break;
    }

    freeaddrinfo(res);
    if (fd < 0) {
        return kErrInternal;
    }

    p->fd = fd;
    p->inited = 1;
    return kOk;
}

Result udp_connect(SocketImpl *s, const char *host_ip, uint16_t port)
{
    SocketImpl *p;
    struct addrinfo hints;
    struct addrinfo *res = NULL;
    struct addrinfo *ai = NULL;
    char port_str[16];

    if (!s || !host_ip) {
        return kErrInvalidArg;
    }
    p = s;
    if (!p->inited || p->fd < 0 || !p->is_dgram) {
        return kErrInvalidArg;
    }

    if (gai_make_port_string(port, port_str) != kOk) {
        return kErrInternal;
    }
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;
    if (getaddrinfo(host_ip, port_str, &hints, &res) != 0) {
        return kErrInvalidArg;
    }
    for (ai = res; ai; ai = ai->ai_next) {
        if (connect(p->fd, ai->ai_addr, (socklen_t)ai->ai_addrlen) == 0) {
            p->family = ai->ai_family;
            freeaddrinfo(res);
            return kOk;
        }
    }
    freeaddrinfo(res);
    return kErrInternal;
    return kOk;
}

Result get_local_port(SocketImpl *s, uint16_t *out_port)
{
    SocketImpl *p;
    struct sockaddr_storage ss;
    socklen_t len = sizeof(ss);

    if (!s || !out_port) {
        return kErrInvalidArg;
    }
    p = s;
    if (!p->inited || p->fd < 0) {
        return kErrInvalidArg;
    }
    memset(&ss, 0, sizeof(ss));
    if (getsockname(p->fd, (struct sockaddr *)&ss, &len) != 0) {
        return kErrInternal;
    }
    if (((struct sockaddr *)&ss)->sa_family == AF_INET) {
        *out_port = ntohs(((struct sockaddr_in *)&ss)->sin_port);
        return kOk;
    }
    if (((struct sockaddr *)&ss)->sa_family == AF_INET6) {
        *out_port = ntohs(((struct sockaddr_in6 *)&ss)->sin6_port);
        return kOk;
    }
    return kErrUnsupported;
    return kOk;
}

Result close(SocketImpl *s)
{
    SocketImpl *p;
    int fd;
    int is_server;
    char path_copy[sizeof(((struct sockaddr_un *)0)->sun_path)];

    if (!s) {
        return kErrInvalidArg;
    }

    p = s;
    fd = p->fd;
    is_server = p->is_server;
    memset(path_copy, 0, sizeof(path_copy));
    strncpy(path_copy, p->path, sizeof(path_copy) - 1);
    memset(s, 0, sizeof(*s));

    if (fd >= 0) {
        (void)::close(fd);
    }

    /* For server sockets, unlink endpoint. */
    if (is_server && path_copy[0] != '\0') {
        (void)unlink(path_copy);
    }
    return kOk;
}

Result accept(SocketImpl *server, SocketImpl *client, int timeout_ms)
{
    SocketImpl *sp;
    SocketImpl *cp;
    struct pollfd fds[1];
    int rc;

    if (!server || !client) {
        return kErrInvalidArg;
    }
    sp = server;
    if (!sp->inited || sp->fd < 0) {
        return kErrInvalidArg;
    }
    if (sp->is_dgram) {
        return kErrUnsupported;
    }

    fds[0].fd = sp->fd;
    fds[0].events = POLLIN;
    fds[0].revents = 0;

    rc = poll(fds, 1, timeout_ms);
    if (rc < 0) {
        if (errno == EINTR) {
            return kErrAgain;
        }
        return kErrInternal;
    }
    if (rc == 0) {
        return kErrTimeout;
    }

    /* accept into client fd */
    memset(client, 0, sizeof(*client));
    cp = client;
    cp->fd = -1;
    cp->inited = 0;

    for (;;) {
        int newfd = ::accept(sp->fd, NULL, NULL);
        if (newfd >= 0) {
            if (set_nonblocking(newfd, 1) != kOk) {
                ::close(newfd);
                return kErrInternal;
            }
            cp->fd = newfd;
            cp->inited = 1;
            cp->is_server = 0;
            return kOk;
        }
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return kErrTimeout;
        }
        if (errno == EINTR) {
            continue;
        }
        return kErrInternal;
    }
}

Result send(SocketImpl *s,
                            const void *buf,
                            size_t len,
                            size_t *out_sent,
                            int timeout_ms)
{
    SocketImpl *p;
    const uint8_t *ptr = (const uint8_t *)buf;
    size_t total = 0;

    if (!s || (!buf && len > 0)) {
        return kErrInvalidArg;
    }

    p = s;
    if (!p->inited || p->fd < 0) {
        return kErrInvalidArg;
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
            return kErrTimeout;
        }
        if (rc < 0) {
            if (errno == EINTR) {
                return kErrAgain;
            }
            return kErrInternal;
        }
        ssize_t n = ::send(p->fd, ptr, len, 0);
        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                return kErrTimeout;
            }
            return kErrInternal;
        }
        total = (size_t)n;
        if (out_sent) {
            *out_sent = total;
        }
        return (total == len) ? kOk : kErrInternal;
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
                return kErrAgain;
            }
            return kErrInternal;
        }
        if (rc == 0) {
            return (timeout_ms >= 0) ? kErrTimeout : kErrInternal;
        }

        ssize_t n = ::send(p->fd, ptr + total, len - total, 0);
        if (n > 0) {
            total += (size_t)n;
            continue;
        }
        if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            if (timeout_ms == 0) {
                return kErrTimeout;
            }
            continue;
        }
        if (n < 0 && errno == EINTR) {
            continue;
        }
        return kErrInternal;
    }

    if (out_sent) {
        *out_sent = total;
    }
    return kOk;
}

Result recv(SocketImpl *s,
                            void *buf,
                            size_t len,
                            size_t *out_received,
                            int timeout_ms)
{
    SocketImpl *p;
    uint8_t *ptr = (uint8_t *)buf;
    size_t total = 0;

    if (!s || (!buf && len > 0)) {
        return kErrInvalidArg;
    }

    p = s;
    if (!p->inited || p->fd < 0) {
        return kErrInvalidArg;
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
            return kErrTimeout;
        }
        if (rc < 0) {
            if (errno == EINTR) {
                return kErrAgain;
            }
            return kErrInternal;
        }
        ssize_t n = ::recv(p->fd, ptr, len, 0);
        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                return kErrTimeout;
            }
            return kErrInternal;
        }
        if (n == 0) {
            return kErrInternal;
        }
        total = (size_t)n;
        if (out_received) {
            *out_received = total;
        }
        return kOk;
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
                return kErrAgain;
            }
            return kErrInternal;
        }
        if (rc == 0) {
            return kErrTimeout;
        }

        ssize_t n = ::recv(p->fd, ptr + total, len - total, 0);
        if (n > 0) {
            total += (size_t)n;
            continue;
        }
        if (n == 0) {
            /* peer closed */
            return kErrInternal;
        }
        if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            if (timeout_ms == 0) {
                return kErrTimeout;
            }
            continue;
        }
        if (n < 0 && errno == EINTR) {
            continue;
        }
        return kErrInternal;
    }

    if (out_received) {
        *out_received = total;
    }
    return kOk;
}

static Result sockaddr_from_sa(SockAddr *out, const struct sockaddr *sa, socklen_t slen)
{
    if (!out || !sa) {
        return kErrInvalidArg;
    }
    if (slen > (socklen_t)sizeof(out->storage)) {
        return kErrNoMem;
    }
    memset(out, 0, sizeof(*out));
    memcpy(out->storage, sa, (size_t)slen);
    out->len = (uint32_t)slen;
    return kOk;
}

Result sendto(SocketImpl *s,
                             const void *buf,
                             size_t len,
                             const SockAddr *to,
                             size_t *out_sent,
                             int timeout_ms)
{
    SocketImpl *p;

    if (out_sent) {
        *out_sent = 0;
    }
    if (!s || (!buf && len > 0) || !out_sent || !to || to->len == 0) {
        return kErrInvalidArg;
    }

    p = s;
    if (!p->inited || p->fd < 0 || !p->is_dgram) {
        return kErrUnsupported;
    }

    struct pollfd fds[1];
    fds[0].fd = p->fd;
    fds[0].events = POLLOUT;
    fds[0].revents = 0;
    int rc = poll(fds, 1, timeout_ms);
    if (rc == 0) {
        return kErrTimeout;
    }
    if (rc < 0) {
        if (errno == EINTR) {
            return kErrAgain;
        }
        return kErrInternal;
    }

    ssize_t n = sendto(p->fd, buf, len, 0, (const struct sockaddr *)to->storage, (socklen_t)to->len);
    if (n < 0) {
        if (errno == EINTR) {
            return kErrAgain;
        }
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return kErrTimeout;
        }
        return kErrInternal;
    }
    *out_sent = (size_t)n;
    return ((size_t)n == len) ? kOk : kErrInternal;
}

Result recvfrom(SocketImpl *s,
                               void *buf,
                               size_t len,
                               size_t *out_received,
                               SockAddr *out_from,
                               int timeout_ms)
{
    SocketImpl *p;
    struct sockaddr_storage ss;
    socklen_t slen = sizeof(ss);

    if (out_received) {
        *out_received = 0;
    }
    if (!s || (!buf && len > 0) || !out_received || !out_from) {
        return kErrInvalidArg;
    }

    p = s;
    if (!p->inited || p->fd < 0 || !p->is_dgram) {
        return kErrUnsupported;
    }

    struct pollfd fds[1];
    fds[0].fd = p->fd;
    fds[0].events = POLLIN;
    fds[0].revents = 0;
    int rc = poll(fds, 1, timeout_ms);
    if (rc == 0) {
        return kErrTimeout;
    }
    if (rc < 0) {
        if (errno == EINTR) {
            return kErrAgain;
        }
        return kErrInternal;
    }

    memset(&ss, 0, sizeof(ss));
    ssize_t n = recvfrom(p->fd, buf, len, 0, (struct sockaddr *)&ss, &slen);
    if (n < 0) {
        if (errno == EINTR) {
            return kErrAgain;
        }
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return kErrTimeout;
        }
        return kErrInternal;
    }
    *out_received = (size_t)n;
    (void)sockaddr_from_sa(out_from, (struct sockaddr *)&ss, slen);
    return kOk;
}


}  // namespace openember::osal::detail
