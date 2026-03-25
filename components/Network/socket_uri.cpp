#include "socket_uri.h"

#include <cstring>

namespace openember::network {

static bool starts_with(std::string_view s, std::string_view p)
{
    return s.size() >= p.size() && s.substr(0, p.size()) == p;
}

oe_result_t parse_endpoint(std::string_view uri, Endpoint &out)
{
    out = Endpoint{};

    if (uri.empty()) {
        return OE_ERR_INVALID_ARG;
    }

    if (starts_with(uri, "unix://")) {
        out.scheme = Scheme::Unix;
        std::string_view rest = uri.substr(std::strlen("unix://"));
        /* Accept unix:///abs/path or unix://relative */
        out.path = std::string(rest);
        if (out.path.empty()) {
            return OE_ERR_INVALID_ARG;
        }
        return OE_OK;
    }

    if (starts_with(uri, "tcp://")) {
        out.scheme = Scheme::Tcp;
        std::string_view rest = uri.substr(std::strlen("tcp://"));
        std::string_view host_sv;
        std::string_view port_sv;
        if (!rest.empty() && rest.front() == '[') {
            auto rb = rest.find(']');
            if (rb == std::string_view::npos || rb + 2 > rest.size() || rest[rb + 1] != ':') {
                return OE_ERR_INVALID_ARG;
            }
            host_sv = rest.substr(1, rb - 1);
            port_sv = rest.substr(rb + 2);
        } else {
            auto pos = rest.rfind(':');
            if (pos == std::string_view::npos) {
                return OE_ERR_INVALID_ARG;
            }
            host_sv = rest.substr(0, pos);
            port_sv = rest.substr(pos + 1);
        }
        out.host = std::string(host_sv);
        if (out.host.empty() || port_sv.empty()) {
            return OE_ERR_INVALID_ARG;
        }
        unsigned long port = 0;
        for (char c : port_sv) {
            if (c < '0' || c > '9') {
                return OE_ERR_INVALID_ARG;
            }
            port = port * 10 + (unsigned long)(c - '0');
            if (port > 65535) {
                return OE_ERR_INVALID_ARG;
            }
        }
        out.port = static_cast<uint16_t>(port);
        return OE_OK;
    }

    if (starts_with(uri, "udp://")) {
        out.scheme = Scheme::Udp;
        std::string_view rest = uri.substr(std::strlen("udp://"));
        std::string_view host_sv;
        std::string_view port_sv;
        if (!rest.empty() && rest.front() == '[') {
            auto rb = rest.find(']');
            if (rb == std::string_view::npos || rb + 2 > rest.size() || rest[rb + 1] != ':') {
                return OE_ERR_INVALID_ARG;
            }
            host_sv = rest.substr(1, rb - 1);
            port_sv = rest.substr(rb + 2);
        } else {
            auto pos = rest.rfind(':');
            if (pos == std::string_view::npos) {
                return OE_ERR_INVALID_ARG;
            }
            host_sv = rest.substr(0, pos);
            port_sv = rest.substr(pos + 1);
        }
        out.host = std::string(host_sv);
        if (out.host.empty() || port_sv.empty()) {
            return OE_ERR_INVALID_ARG;
        }
        unsigned long port = 0;
        for (char c : port_sv) {
            if (c < '0' || c > '9') {
                return OE_ERR_INVALID_ARG;
            }
            port = port * 10 + (unsigned long)(c - '0');
            if (port > 65535) {
                return OE_ERR_INVALID_ARG;
            }
        }
        out.port = static_cast<uint16_t>(port);
        return OE_OK;
    }

    out.scheme = Scheme::Unknown;
    return OE_ERR_UNSUPPORTED;
}

Socket::~Socket()
{
    (void)oe_socket_close(&s_);
}

oe_result_t Socket::close()
{
    return oe_socket_close(&s_);
}

oe_result_t Socket::listen(std::string_view uri, uint32_t backlog)
{
    Endpoint ep;
    oe_result_t r = parse_endpoint(uri, ep);
    if (r != OE_OK) {
        return r;
    }

    switch (ep.scheme) {
    case Scheme::Unix:
        return oe_socket_open_unix_server(&s_, ep.path.c_str(), backlog);
    case Scheme::Tcp:
        return oe_socket_open_tcp_server(&s_, ep.host.empty() ? NULL : ep.host.c_str(), ep.port, backlog);
    case Scheme::Udp:
        return oe_socket_open_udp(&s_, ep.host.empty() ? NULL : ep.host.c_str(), ep.port);
    default:
        return OE_ERR_UNSUPPORTED;
    }
}

oe_result_t Socket::connect(std::string_view uri)
{
    Endpoint ep;
    oe_result_t r = parse_endpoint(uri, ep);
    if (r != OE_OK) {
        return r;
    }

    switch (ep.scheme) {
    case Scheme::Unix:
        return oe_socket_open_unix_client(&s_, ep.path.c_str());
    case Scheme::Tcp:
        return oe_socket_open_tcp_client(&s_, ep.host.c_str(), ep.port, 5000);
    case Scheme::Udp: {
        oe_result_t rr = oe_socket_open_udp(&s_, "0.0.0.0", 0);
        if (rr != OE_OK) {
            return rr;
        }
        return oe_socket_udp_connect(&s_, ep.host.c_str(), ep.port);
    }
    default:
        return OE_ERR_UNSUPPORTED;
    }
}

oe_result_t Socket::accept(Socket &out_client, int timeout_ms)
{
    return oe_socket_accept(&s_, &out_client.s_, timeout_ms);
}

oe_result_t Socket::send(const void *buf, size_t len, size_t *out_sent, int timeout_ms)
{
    return oe_socket_send(&s_, buf, len, out_sent, timeout_ms);
}

oe_result_t Socket::recv(void *buf, size_t len, size_t *out_received, int timeout_ms)
{
    return oe_socket_recv(&s_, buf, len, out_received, timeout_ms);
}

} // namespace openember::network

