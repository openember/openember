/*
 * Copyright (c) 2025-2026, OpenEmber Team
 * SPDX-License-Identifier: Apache-2.0
 *
 * 与 publisher 配合演示 msg_bus 订阅。
 *
 * 用法: ./msgbus_demo_subscriber [msg_bus_address]
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <string>

#include "message.h"
#undef LOG_TAG
#define LOG_TAG "msgbus_demo_sub"
#include "openember/log.h"

#ifndef DEMO_TOPIC_PREFIX
#define DEMO_TOPIC_PREFIX "openember/demo"
#endif

static void on_msg(char *topic, void *payload, size_t payloadlen)
{
    LOG_I("[subscriber] topic=%s payload=%.*s", topic, (int)payloadlen, (const char *)payload);
}

int main(int argc, char **argv)
{
    msg_node_t h{};
    oe_log_init(LOG_TAG);

    std::string addr_buf;
    char *addr = nullptr;
    if (argc > 1 && argv[1][0]) {
        addr_buf = argv[1];
        addr = addr_buf.data();
    }
#ifdef OPENEMBER_MSGBUS_DEFAULT_ADDR
    else if (OPENEMBER_MSGBUS_DEFAULT_ADDR[0]) {
        addr_buf = OPENEMBER_MSGBUS_DEFAULT_ADDR;
        addr = addr_buf.data();
    }
#endif

    int rc = msg_bus_init(&h, "demo_sub", addr, on_msg);
    if (rc != EMBER_EOK) {
        LOG_E("msg_bus_init failed rc=%d", rc);
        return 1;
    }

    rc = msg_bus_subscribe(h, DEMO_TOPIC_PREFIX);
    if (rc != EMBER_EOK) {
        LOG_E("msg_bus_subscribe failed rc=%d", rc);
        msg_bus_deinit(h);
        return 1;
    }

    LOG_I("connected, filter prefix \"%s\" (run msgbus_demo_publisher in another terminal)", DEMO_TOPIC_PREFIX);

    while (1) {
        sleep(3600);
    }
}
