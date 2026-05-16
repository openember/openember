#include "socket_uri.h"

#include <cstring>

namespace openember::network {

using openember::osal::kErrInvalidArg;
using openember::osal::kErrUnsupported;
using openember::osal::kOk;
using openember::osal::Result;

static bool starts_with(std::string_view s, std::string_view p)
{
    return s.size() >= p.size() && s.substr(0, p.size()) == p;
}

Result parse_endpoint(std::string_view uri, Endpoint& out)
{
    out = Endpoint{};

    if (uri.empty()) {
        return kErrInvalidArg;
    }

    if (starts_with(uri, "unix://")) {
        out.scheme = Scheme::Unix;
        const std::string_view rest = uri.substr(std::strlen("unix://"));
        out.path = std::string(rest);
        return out.path.empty() ? kErrInvalidArg : kOk;
    }

    auto parse_host_port = [](std::string_view rest, Endpoint& ep) -> Result {
        std::string_view host_sv;
        std::string_view port_sv;
        if (!rest.empty() && rest.front() == '[') {
            const auto rb = rest.find(']');
            if (rb == std::string_view::npos || rb + 2 > rest.size() || rest[rb + 1] != ':') {
                return kErrInvalidArg;
            }
            host_sv = rest.substr(1, rb - 1);
            port_sv = rest.substr(rb + 2);
        } else {
            const auto pos = rest.rfind(':');
            if (pos == std::string_view::npos) {
                return kErrInvalidArg;
            }
            host_sv = rest.substr(0, pos);
            port_sv = rest.substr(pos + 1);
        }
        if (host_sv.empty() || port_sv.empty()) {
            return kErrInvalidArg;
        }
        unsigned long port = 0;
        for (const char c : port_sv) {
            if (c < '0' || c > '9') {
                return kErrInvalidArg;
            }
            port = port * 10 + static_cast<unsigned long>(c - '0');
            if (port > 65535) {
                return kErrInvalidArg;
            }
        }
        ep.host = std::string(host_sv);
        ep.port = static_cast<uint16_t>(port);
        return kOk;
    };

    if (starts_with(uri, "tcp://")) {
        out.scheme = Scheme::Tcp;
        return parse_host_port(uri.substr(std::strlen("tcp://")), out);
    }

    if (starts_with(uri, "udp://")) {
        out.scheme = Scheme::Udp;
        return parse_host_port(uri.substr(std::strlen("udp://")), out);
    }

    out.scheme = Scheme::Unknown;
    return kErrUnsupported;
}

Result Socket::close()
{
    return socket_.close();
}

Result Socket::listen(std::string_view uri, uint32_t backlog)
{
    Endpoint ep;
    const Result r = parse_endpoint(uri, ep);
    if (r != kOk) {
        return r;
    }

    switch (ep.scheme) {
    case Scheme::Unix:
        return socket_.open_unix_server(ep.path, backlog);
    case Scheme::Tcp:
        return socket_.open_tcp_server(ep.host, ep.port, backlog);
    case Scheme::Udp:
        return socket_.open_udp(ep.host.empty() ? "0.0.0.0" : ep.host, ep.port);
    default:
        return kErrUnsupported;
    }
}

Result Socket::connect(std::string_view uri)
{
    Endpoint ep;
    const Result r = parse_endpoint(uri, ep);
    if (r != kOk) {
        return r;
    }

    switch (ep.scheme) {
    case Scheme::Unix:
        return socket_.open_unix_client(ep.path);
    case Scheme::Tcp:
        return socket_.open_tcp_client(ep.host, ep.port, 5000);
    case Scheme::Udp: {
        const Result rr = socket_.open_udp("0.0.0.0", 0);
        if (rr != kOk) {
            return rr;
        }
        return socket_.udp_connect(ep.host, ep.port);
    }
    default:
        return kErrUnsupported;
    }
}

Result Socket::accept(Socket& out_client, int timeout_ms)
{
    return socket_.accept(out_client.socket_, timeout_ms);
}

Result Socket::send(const void* buf, size_t len, size_t* out_sent, int timeout_ms)
{
    return socket_.send(buf, len, out_sent, timeout_ms);
}

Result Socket::recv(void* buf, size_t len, size_t* out_received, int timeout_ms)
{
    return socket_.recv(buf, len, out_received, timeout_ms);
}

}  // namespace openember::network
