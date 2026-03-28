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
 * C++ 传输抽象（每进程一种 msgbus 后端）。
 *
 * CreateDefaultTransportBackend()：LCM 且 OPENEMBER_MSGBUS_LCM_USE_CPP=ON 时用
 * lcm::LCM（lcm_transport_backend.cpp）；否则用 ClibTransportBackend → msg_bus_*。
 * 直接调用 msg_bus_* 的模块仍走各 *_wrapper.c，与上述 C++ 路径可并存。
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
