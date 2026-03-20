/*
 * Copyright (c) 2026, OpenEmber Team
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "ember_pubsub.h"

#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <lcm/lcm.h>

// LCM pub/sub 本身是“按 channel 路由”，不直接支持 prefix/filter 订阅。
// 为了兼容 ember_pubsub 的 topic_prefix 语义，我们采用：
// - 永远发布到固定 channel（EMBER_PUBSUB_LCM_CHANNEL）
// - 在消息 payload 内编码 [topic][\\0][payload]
// - 订阅端在回调里对 topic 做 prefix 过滤
#define EMBER_PUBSUB_LCM_CHANNEL "openember/pubsub"

#define EMBER_PS_MAX_TOPIC 1024u

struct ember_pubsub {
    lcm_t *lcm;
    int is_publisher;

    pthread_t recv_thread;
    volatile int recv_running;

    ember_pubsub_on_message_t on_message;
    void *user_data;

    char topic_filter[EMBER_PS_MAX_TOPIC];
    size_t filter_len;
};

static void parse_topic_payload(const void *data, size_t data_len, char **out_topic,
                                   const void **out_payload, size_t *out_payload_len)
{
    const unsigned char *p = (const unsigned char *)data;
    size_t i = 0u;

    *out_topic = NULL;
    *out_payload = NULL;
    *out_payload_len = 0u;

    for (i = 0; i < data_len; i++) {
        if (p[i] == 0x00) {
            size_t topic_len = i;
            *out_payload = (const unsigned char *)data + i + 1u;
            *out_payload_len = (data_len > i + 1u) ? (data_len - (i + 1u)) : 0u;

            *out_topic = (char *)malloc(topic_len + 1u);
            if (*out_topic) {
                memcpy(*out_topic, data, topic_len);
                (*out_topic)[topic_len] = '\0';
            }
            return;
        }
    }

    // No delimiter: treat whole as topic.
    *out_payload = NULL;
    *out_payload_len = 0u;
    *out_topic = (char *)malloc(data_len + 1u);
    if (*out_topic) {
        memcpy(*out_topic, data, data_len);
        (*out_topic)[data_len] = '\0';
    }
}

static void lcm_msg_handler(const lcm_recv_buf_t *rbuf, const char *channel, void *user_data)
{
    (void)channel;
    struct ember_pubsub *ps = (struct ember_pubsub *)user_data;
    if (!ps || !ps->on_message || !rbuf || !rbuf->data || rbuf->data_size == 0u) {
        return;
    }

    char *topic_copy = NULL;
    const void *payload_ptr = NULL;
    size_t payload_len = 0u;

    parse_topic_payload(rbuf->data, (size_t)rbuf->data_size, &topic_copy,
                         &payload_ptr, &payload_len);
    if (!topic_copy) {
        return;
    }

    // Filter by prefix (topic_filter is stored in ps).
    if (ps->filter_len > 0u) {
        size_t topic_len = strlen(topic_copy);
        if (topic_len < ps->filter_len ||
            memcmp(topic_copy, ps->topic_filter, ps->filter_len) != 0) {
            free(topic_copy);
            return;
        }
    }

    void *payload_copy = NULL;
    if (payload_len > 0u && payload_ptr) {
        payload_copy = malloc(payload_len);
        if (payload_copy) {
            memcpy(payload_copy, payload_ptr, payload_len);
        }
    }

    ps->on_message(topic_copy, payload_copy, payload_len, ps->user_data);

    free(payload_copy);
    free(topic_copy);
}

static void *recv_loop(void *arg)
{
    struct ember_pubsub *ps = (struct ember_pubsub *)arg;

    while (ps->recv_running) {
        (void)lcm_handle_timeout(ps->lcm, 100);
    }

    return NULL;
}

int ember_pubsub_create_publisher(ember_pubsub_t **out, const char *bind_url)
{
    if (!out || !bind_url) {
        return -EINVAL;
    }

    struct ember_pubsub *ps = (struct ember_pubsub *)calloc(1u, sizeof(*ps));
    if (!ps) {
        return -ENOMEM;
    }

    // LCM provider URL uses the lcm_create() provider string semantics.
    ps->lcm = lcm_create(bind_url);
    if (!ps->lcm) {
        free(ps);
        return -EFAULT;
    }

    ps->is_publisher = 1;
    ps->filter_len = 0u;

    *out = ps;
    return 0;
}

int ember_pubsub_create_subscriber(ember_pubsub_t **out, const char *connect_url,
                                     ember_pubsub_on_message_t on_message, void *user_data)
{
    if (!out || !connect_url) {
        return -EINVAL;
    }

    struct ember_pubsub *ps = (struct ember_pubsub *)calloc(1u, sizeof(*ps));
    if (!ps) {
        return -ENOMEM;
    }

    ps->lcm = lcm_create(connect_url);
    if (!ps->lcm) {
        free(ps);
        return -EFAULT;
    }

    ps->on_message = on_message;
    ps->user_data = user_data;

    // Default filter: empty => receive all.
    ps->filter_len = 0u;

    if (!lcm_subscribe(ps->lcm, EMBER_PUBSUB_LCM_CHANNEL, lcm_msg_handler, ps)) {
        lcm_destroy(ps->lcm);
        free(ps);
        return -EFAULT;
    }

    ps->recv_running = 1;
    if (pthread_create(&ps->recv_thread, NULL, recv_loop, ps) != 0) {
        ps->recv_running = 0;
        lcm_destroy(ps->lcm);
        free(ps);
        return -EAGAIN;
    }

    ps->is_publisher = 0;
    *out = ps;
    return 0;
}

int ember_pubsub_subscribe(ember_pubsub_t *ps, const char *topic_prefix)
{
    if (!ps || !topic_prefix) {
        return -EINVAL;
    }

    size_t len = strlen(topic_prefix);
    if (len >= EMBER_PS_MAX_TOPIC) {
        len = EMBER_PS_MAX_TOPIC - 1u;
    }

    memcpy(((struct ember_pubsub *)ps)->topic_filter, topic_prefix, len);
    ((struct ember_pubsub *)ps)->topic_filter[len] = '\0';
    ((struct ember_pubsub *)ps)->filter_len = len;
    return 0;
}

int ember_pubsub_publish(ember_pubsub_t *ps, const char *topic, const void *payload,
                           size_t payload_len)
{
    if (!ps || !topic) {
        return -EINVAL;
    }
    if (((struct ember_pubsub *)ps)->is_publisher == 0) {
        return -EINVAL;
    }

    struct ember_pubsub *p = (struct ember_pubsub *)ps;

    size_t topic_len = strlen(topic);
    if (topic_len >= EMBER_PS_MAX_TOPIC) {
        return -EINVAL;
    }

    size_t msg_len = topic_len + 1u + payload_len;
    unsigned char *msg = (unsigned char *)malloc(msg_len);
    if (!msg) {
        return -ENOMEM;
    }

    memcpy(msg, topic, topic_len);
    msg[topic_len] = 0x00;
    if (payload_len > 0u && payload) {
        memcpy(msg + topic_len + 1u, payload, payload_len);
    }

    int rc = lcm_publish(p->lcm, EMBER_PUBSUB_LCM_CHANNEL, msg, (unsigned int)msg_len);
    free(msg);
    return (rc >= 0) ? 0 : rc;
}

void ember_pubsub_destroy(ember_pubsub_t *ps)
{
    if (!ps) {
        return;
    }

    struct ember_pubsub *p = (struct ember_pubsub *)ps;

    if (!p->is_publisher) {
        p->recv_running = 0;
        pthread_join(p->recv_thread, NULL);
    }

    if (p->lcm) {
        lcm_destroy(p->lcm);
    }
    free(p);
}

