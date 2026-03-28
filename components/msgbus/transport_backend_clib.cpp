#include "transport_backend.hpp"

#include <cstring>
#include <memory>
#include <string>

extern "C" {
#include "message.h"
}

namespace openember::msgbus {

extern "C" void ClibCbBridge(void *user_data, char *topic, void *payload, size_t payload_len);

class ClibTransportBackend final : public TransportBackend {
public:
    int init(const std::string &node_name, const std::string &address, MessageCallback cb) override
    {
        cb_ = std::move(cb);
        char *addr = nullptr;
        if (!address.empty()) {
            addr_buf_ = address;
            addr = addr_buf_.data();
        }
        return msg_bus_init(&handle_, node_name.c_str(), addr, ClibCbBridge, this);
    }

    int deinit() override
    {
        int rc = 0;
        if (handle_) {
            rc = msg_bus_deinit(handle_);
            handle_ = nullptr;
        }
        cb_ = {};
        return rc;
    }

    int publish(std::string_view topic, std::string_view payload_text) override
    {
        std::string t(topic);
        std::string p(payload_text);
        return msg_bus_publish(handle_, t.c_str(), p.c_str());
    }

    int publish_raw(std::string_view topic, const void *payload, std::size_t payload_len) override
    {
        std::string t(topic);
        return msg_bus_publish_raw(handle_, t.c_str(), payload, (int)payload_len);
    }

    int subscribe(std::string_view topic_prefix) override
    {
        std::string t(topic_prefix);
        return msg_bus_subscribe(handle_, t.c_str());
    }

    int unsubscribe(std::string_view topic_prefix) override
    {
        std::string t(topic_prefix);
        return msg_bus_unsubscribe(handle_, t.c_str());
    }

    ~ClibTransportBackend() override
    {
        (void)deinit();
    }

private:
    friend void ClibCbBridge(void *user_data, char *topic, void *payload, size_t payload_len);

    msg_node_t handle_{nullptr};
    MessageCallback cb_{};
    std::string addr_buf_{};
};

extern "C" void ClibCbBridge(void *user_data, char *topic, void *payload, size_t payload_len)
{
    auto *self = static_cast<ClibTransportBackend *>(user_data);
    if (!self || !self->cb_ || !topic) {
        return;
    }
    self->cb_(topic, payload, payload_len);
}

std::unique_ptr<TransportBackend> detail_create_clib_transport_backend()
{
    return std::make_unique<ClibTransportBackend>();
}

} // namespace openember::msgbus
