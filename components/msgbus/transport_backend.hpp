#ifndef OPENEMBER_MSGBUS_TRANSPORT_BACKEND_HPP
#define OPENEMBER_MSGBUS_TRANSPORT_BACKEND_HPP

#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <string_view>

namespace openember::msgbus {

using MessageCallback = std::function<void(std::string_view topic, const void *payload, std::size_t payload_len)>;

/**
 * C++ facade over the internal msg_bus_* C API (one backend per build).
 *
 * Backend selection is compile-time: CMake sets OPENEMBER_MSGBUS_USE_* and
 * ember_config.h defines EMBER_LIBS_USING_*; the linker resolves msg_bus_* to
 * lcm / nng / mqtt (openember_mqtt) / … See transport_backend_factory.cpp.
 */
class TransportBackend {
public:
    virtual ~TransportBackend() = default;

    virtual int init(const std::string &node_name, const std::string &address, MessageCallback cb) = 0;
    virtual int deinit() = 0;
    virtual int publish(std::string_view topic, std::string_view payload_text) = 0;
    virtual int publish_raw(std::string_view topic, const void *payload, std::size_t payload_len) = 0;
    virtual int subscribe(std::string_view topic_prefix) = 0;
    virtual int unsubscribe(std::string_view topic_prefix) = 0;
};

std::unique_ptr<TransportBackend> CreateDefaultTransportBackend();

} // namespace openember::msgbus

#endif // OPENEMBER_MSGBUS_TRANSPORT_BACKEND_HPP
