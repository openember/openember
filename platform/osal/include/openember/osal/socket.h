/* OpenEmber OSAL — Socket (C ABI, Linux: POSIX sockets)
 *
 * Provides minimal IPC-friendly primitives (Unix domain socket) with timeout support.
 */
#ifndef OPENEMBER_OSAL_SOCKET_H_
#define OPENEMBER_OSAL_SOCKET_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "openember/osal/types.h"

#include <stdint.h>
#include <stddef.h>

typedef struct oe_socket {
    uint8_t opaque[256];
} oe_socket_t;

typedef struct oe_socket_caps {
    uint32_t supports_unix_domain; /* 1 */
    uint32_t supports_tcp;         /* 1 */
    uint32_t supports_udp;         /* 1 */
} oe_socket_caps_t;

typedef struct oe_sockaddr {
    uint8_t storage[128]; /* sockaddr_storage */
    uint32_t len;         /* socklen_t */
} oe_sockaddr_t;

/* timeout_ms <0: block forever; timeout_ms == 0: poll once; timeout_ms >0: up to N ms */
oe_result_t oe_socket_query_caps(oe_socket_caps_t *out_caps);

/* Open a Unix domain server socket at path.
 * Close will also unlink the path by default.
 */
oe_result_t oe_socket_open_unix_server(oe_socket_t *s, const char *path, uint32_t backlog);

/* Open a Unix domain client socket and connect to path. */
oe_result_t oe_socket_open_unix_client(oe_socket_t *s, const char *path);

/* TCP */
oe_result_t oe_socket_open_tcp_server(oe_socket_t *s,
                                      const char *bind_host, /* NULL/"" -> any (AI_PASSIVE); supports IPv4/IPv6/hostname */
                                      uint16_t port,       /* 0 -> ephemeral */
                                      uint32_t backlog);

oe_result_t oe_socket_open_tcp_client(oe_socket_t *s,
                                      const char *host, /* IPv4/IPv6/hostname */
                                      uint16_t port,
                                      int timeout_ms);

/* UDP: bind local endpoint (port 0 -> ephemeral). Then connect() if you want peer default. */
oe_result_t oe_socket_open_udp(oe_socket_t *s,
                               const char *bind_host, /* NULL/"" -> any; supports IPv4/IPv6/hostname */
                               uint16_t port);

oe_result_t oe_socket_udp_connect(oe_socket_t *s, const char *host, uint16_t port);

/* Get local port after bind/listen (TCP/UDP). */
oe_result_t oe_socket_get_local_port(oe_socket_t *s, uint16_t *out_port);

oe_result_t oe_socket_close(oe_socket_t *s);

/* Accept a connection from server into client.
 * timeout semantics apply.
 */
oe_result_t oe_socket_accept(oe_socket_t *server, oe_socket_t *client, int timeout_ms);

/* Send/recv up to len bytes (loop until len is transferred or timeout expires). */
oe_result_t oe_socket_send(oe_socket_t *s,
                            const void *buf,
                            size_t len,
                            size_t *out_sent,
                            int timeout_ms);

oe_result_t oe_socket_recv(oe_socket_t *s,
                            void *buf,
                            size_t len,
                            size_t *out_received,
                            int timeout_ms);

/* UDP datagram send/recv with explicit peer address.
 * Only supported when underlying socket is datagram (UDP).
 */
oe_result_t oe_socket_sendto(oe_socket_t *s,
                             const void *buf,
                             size_t len,
                             const oe_sockaddr_t *to,
                             size_t *out_sent,
                             int timeout_ms);

oe_result_t oe_socket_recvfrom(oe_socket_t *s,
                               void *buf,
                               size_t len,
                               size_t *out_received,
                               oe_sockaddr_t *out_from,
                               int timeout_ms);

#ifdef __cplusplus
}
#endif

#endif /* OPENEMBER_OSAL_SOCKET_H_ */

