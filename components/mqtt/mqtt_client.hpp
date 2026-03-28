/*
 * Copyright (c) 2026, OpenEmber Team
 * SPDX-License-Identifier: Apache-2.0
 *
 * Thin C++ wrapper over MQTT msg_bus_* (Paho / Mosquitto) when EMBER_LIBS_USING_MQTT is set.
 */
#ifndef OPENEMBER_MQTT_CLIENT_HPP
#define OPENEMBER_MQTT_CLIENT_HPP

#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <string_view>

namespace openember::mqtt {

using MessageCallback = std::function<void(std::string_view topic, const void *payload, std::size_t len)>;

struct ClientConfig {
    std::string server_uri{"tcp://localhost:1883"};
    std::string client_id{"openember"};
    std::string username;
    std::string password;
};

class Client {
public:
    explicit Client(ClientConfig cfg = {});
    ~Client();

    Client(const Client &) = delete;
    Client &operator=(const Client &) = delete;

    int connect(MessageCallback on_message = {});
    int disconnect();
    int publish(std::string_view topic, std::string_view payload);
    int publish_raw(std::string_view topic, const void *data, std::size_t len);

private:
    ClientConfig cfg_;
    void *handle_{nullptr};
    MessageCallback on_{};
};

} // namespace openember::mqtt

#endif // OPENEMBER_MQTT_CLIENT_HPP
