#pragma once

#include <cstddef>
#include <cstdint>

#include "openember/osal/types.hpp"

namespace openember::osal::detail {

struct SocketImpl {
    int fd = -1;
    int inited = 0;
    int is_server = 0;
    int is_dgram = 0;
    int family = 0;
    char path[108];
    uint32_t backlog = 0;
};

struct SocketCaps {
    uint32_t supports_unix_domain = 0;
    uint32_t supports_tcp = 0;
    uint32_t supports_udp = 0;
};

struct SockAddr {
    uint8_t storage[128];
    uint32_t len = 0;
};

Result query_caps(SocketCaps* out_caps);

Result open_unix_server(SocketImpl* s, const char* path, uint32_t backlog);
Result open_unix_client(SocketImpl* s, const char* path);
Result open_tcp_server(SocketImpl* s, const char* bind_host, uint16_t port, uint32_t backlog);
Result open_tcp_client(SocketImpl* s, const char* host, uint16_t port, int timeout_ms);
Result open_udp(SocketImpl* s, const char* bind_host, uint16_t port);
Result udp_connect(SocketImpl* s, const char* host, uint16_t port);
Result get_local_port(SocketImpl* s, uint16_t* out_port);
Result close(SocketImpl* s);
Result accept(SocketImpl* server, SocketImpl* client, int timeout_ms);
Result send(SocketImpl* s, const void* buf, size_t len, size_t* out_sent, int timeout_ms);
Result recv(SocketImpl* s, void* buf, size_t len, size_t* out_received, int timeout_ms);
Result sendto(SocketImpl* s,
              const void* buf,
              size_t len,
              const SockAddr* to,
              size_t* out_sent,
              int timeout_ms);
Result recvfrom(SocketImpl* s,
                void* buf,
                size_t len,
                size_t* out_received,
                SockAddr* out_from,
                int timeout_ms);

}  // namespace openember::osal::detail
