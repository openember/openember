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
    /** tcp://host:1883 明文；ssl://host:8883 TLS（EMQX Cloud 8883 须用 ssl://，勿用 tcp://） */
    std::string server_uri{"tcp://localhost:1883"};
    std::string client_id{"openember"};
    std::string username;
    std::string password;
    /** PEM 根证书路径（EMQX 控制台下载的 CA）；空则依赖系统默认信任库（或配合 ssl_verify_peer=false） */
    std::string ssl_trust_store;
    /** 校验服务端证书；false 仅建议本机调试（等同跳过校验） */
    bool ssl_verify_peer{true};
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

/**
 * 解释 Client::connect() 非零返回值（正数多为对 Paho 错误码取负再取反，例如 BAD_PROTOCOL 对应 14）。
 * 未知时返回 nullptr。
 */
const char *connect_return_hint(int connect_ret);

} // namespace openember::mqtt

#endif // OPENEMBER_MQTT_CLIENT_HPP
