/*
 * Copyright (c) 2025-2026, OpenEmber Team
 * SPDX-License-Identifier: Apache-2.0
 *
 * 与 subscriber 配合演示 msg_bus 发布（与框架同一套传输）。
 *
 * 用法: ./msgbus_demo_publisher [msg_bus_address]
 * 未传地址时使用默认（与构建时 msgbus 后端一致，见 CMake 宏）。
 */

#include <cstdio>
#include <cstring>
#include <unistd.h>

#include <string>

#include "message.h"
#undef LOG_TAG
#define LOG_TAG "msgbus_demo_pub"
#include "openember/log.h"

#ifndef DEMO_TOPIC
#define DEMO_TOPIC "openember/demo/hello"
#endif

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

    int rc = msg_bus_init(&h, "demo_pub", addr, nullptr);
    if (rc != EMBER_EOK) {
        LOG_E("msg_bus_init failed rc=%d", rc);
        return 1;
    }

    /* NNG 等存在「慢加入者」现象，稍等再发首包 */
    usleep(300 * 1000);

    const char *base = "hello from publisher node";
    for (int i = 0; i < 5; i++) {
        char buf[160];
        snprintf(buf, sizeof(buf), "[%d] %s", i, base);
        rc = msg_bus_publish(h, DEMO_TOPIC, buf);
        if (rc != EMBER_EOK) {
            LOG_E("msg_bus_publish failed rc=%d", rc);
        } else {
            LOG_I("published topic=%s body=%s", DEMO_TOPIC, buf);
        }
        sleep(1);
    }

    msg_bus_deinit(h);
    return 0;
}
