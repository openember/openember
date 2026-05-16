#include "openember/osal/socket.hpp"

#include <cstring>

#include "openember/osal/detail/socket_ops.hpp"

namespace openember::osal {

struct Socket::Impl : detail::SocketImpl {};

Socket::Socket() : impl_(std::make_unique<Impl>())
{
    std::memset(impl_.get(), 0, sizeof(detail::SocketImpl));
    impl_->fd = -1;
}

Socket::~Socket()
{
    if (impl_) {
        (void)detail::close(impl_.get());
    }
}

Result Socket::query_caps(SocketCaps* out_caps)
{
    if (!out_caps) {
        return kErrInvalidArg;
    }
    detail::SocketCaps caps {};
    const Result r = detail::query_caps(&caps);
    if (r != kOk) {
        return r;
    }
    out_caps->supports_unix_domain = caps.supports_unix_domain;
    out_caps->supports_tcp = caps.supports_tcp;
    out_caps->supports_udp = caps.supports_udp;
    return kOk;
}

Result Socket::open_unix_server(const std::string& path, uint32_t backlog)
{
    std::memset(impl_.get(), 0, sizeof(detail::SocketImpl));
    impl_->fd = -1;
    return detail::open_unix_server(impl_.get(), path.c_str(), backlog);
}

Result Socket::open_unix_client(const std::string& path)
{
    std::memset(impl_.get(), 0, sizeof(detail::SocketImpl));
    impl_->fd = -1;
    return detail::open_unix_client(impl_.get(), path.c_str());
}

Result Socket::open_tcp_server(const std::string& bind_host, uint16_t port, uint32_t backlog)
{
    std::memset(impl_.get(), 0, sizeof(detail::SocketImpl));
    impl_->fd = -1;
    const char* host = bind_host.empty() ? nullptr : bind_host.c_str();
    return detail::open_tcp_server(impl_.get(), host, port, backlog);
}

Result Socket::open_tcp_client(const std::string& host, uint16_t port, int timeout_ms)
{
    std::memset(impl_.get(), 0, sizeof(detail::SocketImpl));
    impl_->fd = -1;
    return detail::open_tcp_client(impl_.get(), host.c_str(), port, timeout_ms);
}

Result Socket::open_udp(const std::string& bind_host, uint16_t port)
{
    std::memset(impl_.get(), 0, sizeof(detail::SocketImpl));
    impl_->fd = -1;
    const char* host = bind_host.empty() ? nullptr : bind_host.c_str();
    return detail::open_udp(impl_.get(), host, port);
}

Result Socket::udp_connect(const std::string& host, uint16_t port)
{
    return detail::udp_connect(impl_.get(), host.c_str(), port);
}

Result Socket::get_local_port(uint16_t* out_port)
{
    return detail::get_local_port(impl_.get(), out_port);
}

Result Socket::close()
{
    return detail::close(impl_.get());
}

Result Socket::accept(Socket& client, int timeout_ms)
{
    std::memset(client.impl_.get(), 0, sizeof(detail::SocketImpl));
    client.impl_->fd = -1;
    return detail::accept(impl_.get(), client.impl_.get(), timeout_ms);
}

Result Socket::send(const void* buf, size_t len, size_t* out_sent, int timeout_ms)
{
    return detail::send(impl_.get(), buf, len, out_sent, timeout_ms);
}

Result Socket::recv(void* buf, size_t len, size_t* out_received, int timeout_ms)
{
    return detail::recv(impl_.get(), buf, len, out_received, timeout_ms);
}

Result Socket::sendto(const void* buf, size_t len, const SockAddr* to, size_t* out_sent, int timeout_ms)
{
    const detail::SockAddr* native = reinterpret_cast<const detail::SockAddr*>(to);
    return detail::sendto(impl_.get(), buf, len, native, out_sent, timeout_ms);
}

Result Socket::recvfrom(void* buf, size_t len, size_t* out_received, SockAddr* out_from, int timeout_ms)
{
    detail::SockAddr* native = reinterpret_cast<detail::SockAddr*>(out_from);
    return detail::recvfrom(impl_.get(), buf, len, out_received, native, timeout_ms);
}

}  // namespace openember::osal
