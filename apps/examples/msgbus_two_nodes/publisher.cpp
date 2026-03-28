/*
 * Copyright (c) 2025-2026, OpenEmber Team
 * SPDX-License-Identifier: Apache-2.0
 *
 * 与 subscriber 配合演示 MsgBusNode（C++ 门面，底层仍为当前 msgbus 后端）。
 *
 * 用法: ./msgbus_demo_publisher [msg_bus_address]
 */

#include <cstdio>
#include <cstring>
#include <unistd.h>

#include <string>
#include <string_view>

#include "msgbus_node.hpp"
#include "openember.h"
#undef LOG_TAG
#define LOG_TAG "msgbus_demo_pub"
#include "openember/log.h"

#ifndef DEMO_TOPIC
#define DEMO_TOPIC "openember/demo/hello"
#endif

int main(int argc, char **argv)
{
    oe_log_init(LOG_TAG);

    openember::msgbus::MsgBusNode node("demo_pub");

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

    int rc = node.open(addr_sv, {});
    if (rc != EMBER_EOK) {
        LOG_E("MsgBusNode::open failed rc=%d", rc);
        return 1;
    }

    usleep(300 * 1000);

    const char *base = "hello from publisher node";
    for (int i = 0; i < 5; i++) {
        char buf[160];
        snprintf(buf, sizeof(buf), "[%d] %s", i, base);
        rc = node.publish(DEMO_TOPIC, buf);
        if (rc != EMBER_EOK) {
            LOG_E("publish failed rc=%d", rc);
        } else {
            LOG_I("published topic=%s body=%s", DEMO_TOPIC, buf);
        }
        sleep(1);
    }

    node.close();
    return 0;
}
