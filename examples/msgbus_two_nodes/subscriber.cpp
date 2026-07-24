/*
 * Copyright (c) 2025-2026, OpenEmber Team
 * SPDX-License-Identifier: Apache-2.0
 *
 * 与 publisher 配合演示 MsgBusNode 订阅。
 *
 * 用法: ./msgbus_demo_subscriber [msg_bus_address]
 */

#include <string.h>
#include <unistd.h>

#include <string>
#include <string_view>

#include "msgbus_node.hpp"
#include "openember.h"
#undef LOG_TAG
#define LOG_TAG "msgbus_demo_sub"
#include "openember/log.h"

#ifndef DEMO_TOPIC_PREFIX
#define DEMO_TOPIC_PREFIX "openember/demo"
#endif

int main(int argc, char **argv)
{
    oe_log_init(LOG_TAG);

    openember::msgbus::MsgBusNode node("demo_sub");

    std::string addr_buf;
    std::string_view addr_sv;
    if (argc > 1 && argv[1][0]) {
        addr_buf = argv[1];
        addr_sv = addr_buf;
    }
#ifdef OPENEMBER_MSGBUS_DEFAULT_ADDR
    else if (OPENEMBER_MSGBUS_DEFAULT_ADDR[0]) {
        addr_buf = OPENEMBER_MSGBUS_DEFAULT_ADDR;
        addr_sv = addr_buf;
    }
#endif

    auto on_msg = [](std::string_view topic, const void *payload, std::size_t payload_len) {
        std::string t(topic);
        LOG_I("[subscriber] topic=%s payload=%.*s", t.c_str(), (int)payload_len,
              payload ? (const char *)payload : "");
    };

    int rc = node.open(addr_sv, std::move(on_msg));
    if (rc != EMBER_EOK) {
        LOG_E("MsgBusNode::open failed rc=%d", rc);
        return 1;
    }

    rc = node.subscribe(DEMO_TOPIC_PREFIX);
    if (rc != EMBER_EOK) {
        LOG_E("subscribe failed rc=%d", rc);
        node.close();
        return 1;
    }

    LOG_I("connected, filter prefix \"%s\" (run msgbus_demo_publisher in another terminal)", DEMO_TOPIC_PREFIX);

    while (1) {
        sleep(3600);
    }
}
