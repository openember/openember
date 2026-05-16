#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

#include "openember/osal/types.hpp"

namespace openember::osal {

struct SocketCaps {
    uint32_t supports_unix_domain = 0;
    uint32_t supports_tcp = 0;
    uint32_t supports_udp = 0;
};

struct SockAddr {
    uint8_t storage[128] {};
    uint32_t len = 0;
};

class Socket {
public:
    Socket();
    ~Socket();

    Socket(const Socket&) = delete;
    Socket& operator=(const Socket&) = delete;
    Socket(Socket&&) = delete;
    Socket& operator=(Socket&&) = delete;

    static Result query_caps(SocketCaps* out_caps);

    Result open_unix_server(const std::string& path, uint32_t backlog = 16);
    Result open_unix_client(const std::string& path);
    Result open_tcp_server(const std::string& bind_host, uint16_t port, uint32_t backlog = 16);
    Result open_tcp_client(const std::string& host, uint16_t port, int timeout_ms = 5000);
    Result open_udp(const std::string& bind_host, uint16_t port);
    Result udp_connect(const std::string& host, uint16_t port);
    Result get_local_port(uint16_t* out_port);
    Result close();
    Result accept(Socket& client, int timeout_ms);
    Result send(const void* buf, size_t len, size_t* out_sent, int timeout_ms);
    Result recv(void* buf, size_t len, size_t* out_received, int timeout_ms);
    Result sendto(const void* buf, size_t len, const SockAddr* to, size_t* out_sent, int timeout_ms);
    Result recvfrom(void* buf, size_t len, size_t* out_received, SockAddr* out_from, int timeout_ms);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace openember::osal
