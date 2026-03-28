/*
 * Copyright (c) 2026, OpenEmber Team
 * SPDX-License-Identifier: Apache-2.0
 *
 * C++ 门面：封装 TransportBackend，供应用使用（替代直接调用 msg_bus_* C API）。
 */
#ifndef OPENEMBER_MSGBUS_NODE_HPP
#define OPENEMBER_MSGBUS_NODE_HPP

#include "transport_backend.hpp"

#include <memory>
#include <string>
#include <string_view>

namespace openember::msgbus {

/**
 * 单节点消息总线句柄：与当前编译期选定的后端（LCM/NNG/…）一一对应。
 */
class MsgBusNode {
public:
    explicit MsgBusNode(std::string node_name);
    ~MsgBusNode();

    MsgBusNode(const MsgBusNode &) = delete;
    MsgBusNode &operator=(const MsgBusNode &) = delete;

    using OnMessage = MessageCallback;

    /**
     * @param transport_address LCM: lcm_create provider，如 udpm://…；NNG: 可为空用默认
     * @param on_message 收到匹配订阅前缀的消息时调用（发布端可省略）
     */
    int open(std::string_view transport_address = {}, OnMessage on_message = {});
    void close();

    int publish(std::string_view topic, std::string_view payload);
    int publish_raw(std::string_view topic, const void *data, std::size_t len);
    int subscribe(std::string_view topic_prefix);
    int unsubscribe(std::string_view topic_prefix);

    bool is_open() const { return static_cast<bool>(backend_); }

private:
    std::string node_name_;
    std::unique_ptr<TransportBackend> backend_;
};

} // namespace openember::msgbus

#endif // OPENEMBER_MSGBUS_NODE_HPP
