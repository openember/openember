/*
 * Copyright (c) 2026, OpenEmber Team
 * SPDX-License-Identifier: Apache-2.0
 *
 * MQTT 客户端 C++ 封装（实现：Eclipse Paho MQTT C 同步客户端）。
 * 与框架 msg_bus_* 无关。更换 Broker SDK 时可替换 .cpp 中的 Impl，保持本 API。
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
    /** 例如 tcp://host:1883 或 ssl://host:8883（TLS 需在实现中配置信任库） */
    std::string server_uri{"tcp://localhost:1883"};
    std::string client_id{"openember"};
    std::string username;
    std::string password;
    int keepalive_interval{20};
    int qos_publish{1};
    int qos_subscribe{1};
    int operation_timeout_ms{10000};
};

class Client {
public:
    explicit Client(ClientConfig cfg = {});
    ~Client();

    Client(const Client &) = delete;
    Client &operator=(const Client &) = delete;

    int connect(MessageCallback on_message = {});
    int disconnect();

    bool is_connected() const;

    int publish(std::string_view topic, std::string_view payload);
    int publish_raw(std::string_view topic, const void *data, std::size_t len);

    int subscribe(std::string_view topic, int qos = -1);
    int unsubscribe(std::string_view topic);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace openember::mqtt

#endif // OPENEMBER_MQTT_CLIENT_HPP
