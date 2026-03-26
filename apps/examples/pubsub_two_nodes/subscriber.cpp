/*
 * Copyright (c) 2025, OpenEmber Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * 订阅节点示例：与 publisher 进程配合演示 pub/sub。
 *
 * 用法: ./pubsub_subscriber [connect_url]
 * 默认 connect_url: tcp://127.0.0.1:5560
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "ember_pubsub.h"
#define LOG_TAG "pubsub_subscriber"
#include "openember/log.h"

#ifndef OPENEMBER_PUBSUB_DEFAULT_SUB_URL
#define OPENEMBER_PUBSUB_DEFAULT_SUB_URL "udp://127.0.0.1:7560"
#endif
#define SUB_CONNECT_DEFAULT OPENEMBER_PUBSUB_DEFAULT_SUB_URL

#ifndef DEMO_TOPIC_PREFIX
#define DEMO_TOPIC_PREFIX "openember/demo"
#endif

static void on_message(const char *topic, const void *payload, size_t payload_len, void *user_data)
{
    (void)user_data;
    LOG_I("[subscriber] topic=%s payload=%.*s", topic, (int)payload_len, (const char *)payload);
}

int main(int argc, char **argv)
{
    const char *connect_url = (argc > 1) ? argv[1] : SUB_CONNECT_DEFAULT;
    ember_pubsub_t *sub = NULL;

    oe_log_init(LOG_TAG);

    int rc = ember_pubsub_create_subscriber(&sub, connect_url, on_message, NULL);
    if (rc != 0) {
        LOG_E("ember_pubsub_create_subscriber(%s) failed rc=%d", connect_url, rc);
        return 1;
    }

    if (ember_pubsub_subscribe(sub, DEMO_TOPIC_PREFIX) != 0) {
        LOG_E("ember_pubsub_subscribe failed");
        ember_pubsub_destroy(sub);
        return 1;
    }

    LOG_I("connected to %s, filter prefix \"%s\"", connect_url, DEMO_TOPIC_PREFIX);
    LOG_I("waiting for messages (run pubsub_publisher in another terminal)...");

    /* 保持进程存活以便持续收消息；示例 publisher 只发 5 条，可多次运行 publisher 测试 */
    while (1) {
        sleep(3600);
    }
}

