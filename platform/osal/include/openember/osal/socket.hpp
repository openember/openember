/* OpenEmber OSAL — C++ wrapper: Socket (RAII) */
#ifndef OPENEMBER_OSAL_SOCKET_HPP_
#define OPENEMBER_OSAL_SOCKET_HPP_

#include "openember/osal/socket.h"

#include <string>

namespace oe {
namespace osal {

class Socket {
public:
    Socket() = default;
    Socket(const Socket &) = delete;
    Socket &operator=(const Socket &) = delete;

    ~Socket()
    {
        (void)oe_socket_close(&s_);
    }

    oe_result_t query_caps(oe_socket_caps_t *out_caps) const
    {
        return oe_socket_query_caps(out_caps);
    }

    oe_result_t open_unix_server(const std::string &path, uint32_t backlog = 16)
    {
        return oe_socket_open_unix_server(&s_, path.c_str(), backlog);
    }

    oe_result_t open_unix_client(const std::string &path)
    {
        return oe_socket_open_unix_client(&s_, path.c_str());
    }

    oe_result_t accept(Socket &client, int timeout_ms)
    {
        return oe_socket_accept(&s_, &client.s_, timeout_ms);
    }

    oe_result_t send(const void *buf, size_t len, size_t *out_sent, int timeout_ms)
    {
        return oe_socket_send(&s_, buf, len, out_sent, timeout_ms);
    }

    oe_result_t recv(void *buf, size_t len, size_t *out_received, int timeout_ms)
    {
        return oe_socket_recv(&s_, buf, len, out_received, timeout_ms);
    }

    oe_result_t sendto(const void *buf, size_t len, const oe_sockaddr_t *to, size_t *out_sent, int timeout_ms)
    {
        return oe_socket_sendto(&s_, buf, len, to, out_sent, timeout_ms);
    }

    oe_result_t recvfrom(void *buf,
                         size_t len,
                         size_t *out_received,
                         oe_sockaddr_t *out_from,
                         int timeout_ms)
    {
        return oe_socket_recvfrom(&s_, buf, len, out_received, out_from, timeout_ms);
    }

    oe_result_t close()
    {
        return oe_socket_close(&s_);
    }

private:
    oe_socket_t s_ {};
};

} // namespace osal
} // namespace oe

#endif /* OPENEMBER_OSAL_SOCKET_HPP_ */

