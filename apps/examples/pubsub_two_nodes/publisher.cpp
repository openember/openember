/*
 * Copyright (c) 2025, OpenEmber Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * 发布节点示例：与 subscriber 进程配合演示 pub/sub。
 *
 * 用法: ./pubsub_publisher [bind_url]
 * 默认 bind_url: tcp://*:5560
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "ember_pubsub.h"
#define LOG_TAG "pubsub_publisher"
#include "openember/log.h"

#ifndef OPENEMBER_PUBSUB_DEFAULT_PUB_URL
#define OPENEMBER_PUBSUB_DEFAULT_PUB_URL "udp://127.0.0.1:7560"
#endif
#define PUB_BIND_DEFAULT OPENEMBER_PUBSUB_DEFAULT_PUB_URL

#ifndef DEMO_TOPIC
#define DEMO_TOPIC "openember/demo/hello"
#endif

int main(int argc, char **argv)
{
    const char *bind_url = (argc > 1) ? argv[1] : PUB_BIND_DEFAULT;
    ember_pubsub_t *pub = NULL;

    oe_log_init(LOG_TAG);

    int rc = ember_pubsub_create_publisher(&pub, bind_url);
    if (rc != 0) {
        LOG_E("ember_pubsub_create_publisher(%s) failed rc=%d", bind_url, rc);
        return 1;
    }

    /* ZMQ PUB/SUB 存在「慢加入者」现象，稍等再发首包 */
    usleep(300 * 1000);

    const char *base = "hello from publisher node";
    for (int i = 0; i < 5; i++) {
        char buf[160];
        snprintf(buf, sizeof(buf), "[%d] %s", i, base);
        if (ember_pubsub_publish(pub, DEMO_TOPIC, buf, strlen(buf)) != 0) {
            LOG_E("ember_pubsub_publish failed");
        } else {
            LOG_I("published topic=%s body=%s", DEMO_TOPIC, buf);
        }
        sleep(1);
    }

    ember_pubsub_destroy(pub);
    return 0;
}

