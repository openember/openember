/* OpenEmber Components/Network — High-level socket wrapper (C++17)
 *
 * A lightweight higher-level API built on platform/osal sockets.
 * It parses URIs like:
 *   - unix:///tmp/oe.sock
 *   - tcp://127.0.0.1:1234   (currently unsupported in OSAL, returns OE_ERR_UNSUPPORTED)
 *   - udp://127.0.0.1:1234   (currently unsupported in OSAL, returns OE_ERR_UNSUPPORTED)
 */
#ifndef OPENEMBER_COMPONENTS_NETWORK_SOCKET_URI_H_
#define OPENEMBER_COMPONENTS_NETWORK_SOCKET_URI_H_

#include "openember/osal/socket.h"
#include "openember/osal/types.h"

#include <cstdint>
#include <string>
#include <string_view>

namespace openember::network {

enum class Scheme {
    Unix,
    Tcp,
    Udp,
    Unknown,
};

struct Endpoint {
    Scheme scheme = Scheme::Unknown;
    std::string host; /* for tcp/udp */
    uint16_t port = 0;
    std::string path; /* for unix */
};

oe_result_t parse_endpoint(std::string_view uri, Endpoint &out);

class Socket {
public:
    Socket() = default;
    Socket(const Socket &) = delete;
    Socket &operator=(const Socket &) = delete;

    Socket(Socket &&) = delete;
    Socket &operator=(Socket &&) = delete;

    ~Socket();

    oe_result_t close();

    oe_result_t listen(std::string_view uri, uint32_t backlog = 16);
    oe_result_t connect(std::string_view uri);

    oe_result_t accept(Socket &out_client, int timeout_ms);

    oe_result_t send(const void *buf, size_t len, size_t *out_sent, int timeout_ms);
    oe_result_t recv(void *buf, size_t len, size_t *out_received, int timeout_ms);

private:
    oe_socket_t s_ {};
};

} // namespace openember::network

#endif /* OPENEMBER_COMPONENTS_NETWORK_SOCKET_URI_H_ */

