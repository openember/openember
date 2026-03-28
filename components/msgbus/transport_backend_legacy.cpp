#include "transport_backend.hpp"

#include <cstring>
#include <memory>
#include <string>

extern "C" {
#include "message.h"
}

namespace openember::msgbus {

namespace {

thread_local MessageCallback *g_tls_cb = nullptr;

void LegacyCbBridge(char *topic, void *payload, size_t payload_len)
{
    if (!g_tls_cb || !(*g_tls_cb) || !topic) return;
    (*g_tls_cb)(topic, payload, payload_len);
}

class LegacyTransportBackend final : public TransportBackend {
public:
    int init(const std::string &node_name, const std::string &address, MessageCallback cb) override
    {
        cb_ = std::move(cb);
        g_tls_cb = &cb_;
        char *addr = nullptr;
        if (!address.empty()) {
            addr_buf_ = address;
            addr = addr_buf_.data();
        }
        return msg_bus_init(&handle_, node_name.c_str(), addr, LegacyCbBridge);
    }

    int deinit() override
    {
        int rc = 0;
        if (handle_) {
            rc = msg_bus_deinit(handle_);
            handle_ = nullptr;
        }
        g_tls_cb = nullptr;
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

    ~LegacyTransportBackend() override
    {
        (void)deinit();
    }

private:
    msg_node_t handle_{nullptr};
    MessageCallback cb_{};
    std::string addr_buf_{};
};

} // namespace

std::unique_ptr<TransportBackend> CreateDefaultTransportBackend()
{
    return std::make_unique<LegacyTransportBackend>();
}

} // namespace openember::msgbus
