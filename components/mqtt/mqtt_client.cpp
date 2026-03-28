#include "mqtt_client.hpp"

#include "openember.h"

#ifdef EMBER_LIBS_USING_MQTT
extern "C" {
#include "mqtt_wrapper.h"
}
#endif

#include <cerrno>
#include <cstring>
#include <string>

namespace openember::mqtt {

extern "C" void openember_mqtt_client_bridge(void *user_data, char *topic, void *payload, std::size_t len)
{
    auto *self = static_cast<Client *>(user_data);
    if (!self || !self->on_) {
        return;
    }
    self->on_(topic ? std::string_view(topic) : std::string_view{}, payload, len);
}

Client::Client(ClientConfig cfg) : cfg_(std::move(cfg)) {}

Client::~Client()
{
    (void)disconnect();
}

int Client::connect(MessageCallback on_message)
{
    on_ = std::move(on_message);
#ifndef EMBER_LIBS_USING_MQTT
    (void)on_;
    return -ENOTSUP;
#else
    if (handle_) {
        return -EBUSY;
    }
    char *addr = nullptr;
    std::string addr_buf;
    if (!cfg_.server_uri.empty()) {
        addr_buf = cfg_.server_uri;
        addr = addr_buf.data();
    }
    msg_arrived_cb_t *cb = on_ ? openember_mqtt_client_bridge : nullptr;
    return msg_bus_init(&handle_, cfg_.client_id.c_str(), addr, cb, this);
#endif
}

int Client::disconnect()
{
#ifndef EMBER_LIBS_USING_MQTT
    return -ENOTSUP;
#else
    int rc = 0;
    if (handle_) {
        rc = msg_bus_deinit(handle_);
        handle_ = nullptr;
    }
    on_ = {};
    return rc;
#endif
}

int Client::publish(std::string_view topic, std::string_view payload)
{
#ifndef EMBER_LIBS_USING_MQTT
    return -ENOTSUP;
#else
    if (!handle_) {
        return -EINVAL;
    }
    std::string t(topic);
    std::string p(payload);
    return msg_bus_publish(handle_, t.c_str(), p.c_str());
#endif
}

int Client::publish_raw(std::string_view topic, const void *data, std::size_t len)
{
#ifndef EMBER_LIBS_USING_MQTT
    return -ENOTSUP;
#else
    if (!handle_) {
        return -EINVAL;
    }
    std::string t(topic);
    return msg_bus_publish_raw(handle_, t.c_str(), data, (int)len);
#endif
}

} // namespace openember::mqtt
