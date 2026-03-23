/*
 * Copyright (c) 2025, OpenEmber Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * ZeroMQ PUB/SUB 实现（需系统安装 libzmq）。
 */

#include "ember_pubsub.h"

#include <errno.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <zmq.h>

struct ember_pubsub {
    void *ctx;
    void *sock;
    int is_publisher;
    pthread_t recv_thread;
    volatile int recv_running;
    ember_pubsub_on_message_t on_message;
    void *user_data;
    int default_subscribe_all;
};

static void *recv_loop(void *arg)
{
    ember_pubsub_t *ps = (ember_pubsub_t *)arg;

    while (ps->recv_running) {
        zmq_pollitem_t items[] = {
            {ps->sock, 0, ZMQ_POLLIN, 0},
        };
        int pr = zmq_poll(items, 1, 250);
        if (pr < 0) {
            if (errno == EINTR) {
                continue;
            }
            break;
        }
        if (!(items[0].revents & ZMQ_POLLIN)) {
            continue;
        }

        zmq_msg_t topic_msg;
        zmq_msg_t payload_msg;
        zmq_msg_init(&topic_msg);
        if (zmq_msg_recv(&topic_msg, ps->sock, 0) < 0) {
            zmq_msg_close(&topic_msg);
            continue;
        }
        if (zmq_msg_more(&topic_msg) == 0) {
            zmq_msg_close(&topic_msg);
            continue;
        }

        zmq_msg_init(&payload_msg);
        if (zmq_msg_recv(&payload_msg, ps->sock, 0) < 0) {
            zmq_msg_close(&topic_msg);
            zmq_msg_close(&payload_msg);
            continue;
        }

        size_t tlen = zmq_msg_size(&topic_msg);
        char *topic_copy = (char *)malloc(tlen + 1u);
        if (!topic_copy) {
            zmq_msg_close(&topic_msg);
            zmq_msg_close(&payload_msg);
            continue;
        }
        memcpy(topic_copy, zmq_msg_data(&topic_msg), tlen);
        topic_copy[tlen] = '\0';

        size_t plen = zmq_msg_size(&payload_msg);
        void *payload_copy = malloc(plen ? plen : 1u);
        if (!payload_copy) {
            free(topic_copy);
            zmq_msg_close(&topic_msg);
            zmq_msg_close(&payload_msg);
            continue;
        }
        if (plen > 0) {
            memcpy(payload_copy, zmq_msg_data(&payload_msg), plen);
        }

        zmq_msg_close(&topic_msg);
        zmq_msg_close(&payload_msg);

        if (ps->on_message) {
            ps->on_message(topic_copy, payload_copy, plen, ps->user_data);
        }
        free(topic_copy);
        free(payload_copy);
    }

    return NULL;
}

int ember_pubsub_create_publisher(ember_pubsub_t **out, const char *bind_url)
{
    if (!out || !bind_url) {
        return -EINVAL;
    }

    ember_pubsub_t *ps = (ember_pubsub_t *)calloc(1, sizeof(*ps));
    if (!ps) {
        return -ENOMEM;
    }

    ps->ctx = zmq_ctx_new();
    if (!ps->ctx) {
        free(ps);
        return -1;
    }

    ps->sock = zmq_socket(ps->ctx, ZMQ_PUB);
    if (!ps->sock) {
        zmq_ctx_term(ps->ctx);
        free(ps);
        return -1;
    }

    int linger = 0;
    (void)zmq_setsockopt(ps->sock, ZMQ_LINGER, &linger, sizeof(linger));

    if (zmq_bind(ps->sock, bind_url) != 0) {
        zmq_close(ps->sock);
        zmq_ctx_term(ps->ctx);
        free(ps);
        return -1;
    }

    ps->is_publisher = 1;
    ps->default_subscribe_all = 0;
    *out = ps;
    return 0;
}

int ember_pubsub_create_subscriber(ember_pubsub_t **out, const char *connect_url,
                                   ember_pubsub_on_message_t on_message, void *user_data)
{
    if (!out || !connect_url) {
        return -EINVAL;
    }

    ember_pubsub_t *ps = (ember_pubsub_t *)calloc(1, sizeof(*ps));
    if (!ps) {
        return -ENOMEM;
    }

    ps->on_message = on_message;
    ps->user_data = user_data;
    ps->default_subscribe_all = 1;

    ps->ctx = zmq_ctx_new();
    if (!ps->ctx) {
        free(ps);
        return -1;
    }

    ps->sock = zmq_socket(ps->ctx, ZMQ_SUB);
    if (!ps->sock) {
        zmq_ctx_term(ps->ctx);
        free(ps);
        return -1;
    }

    int linger = 0;
    (void)zmq_setsockopt(ps->sock, ZMQ_LINGER, &linger, sizeof(linger));

    if (zmq_setsockopt(ps->sock, ZMQ_SUBSCRIBE, "", 0) != 0) {
        zmq_close(ps->sock);
        zmq_ctx_term(ps->ctx);
        free(ps);
        return -1;
    }

    if (zmq_connect(ps->sock, connect_url) != 0) {
        zmq_close(ps->sock);
        zmq_ctx_term(ps->ctx);
        free(ps);
        return -1;
    }

    ps->is_publisher = 0;
    ps->recv_running = 1;
    if (pthread_create(&ps->recv_thread, NULL, recv_loop, ps) != 0) {
        ps->recv_running = 0;
        zmq_close(ps->sock);
        zmq_ctx_term(ps->ctx);
        free(ps);
        return -1;
    }

    *out = ps;
    return 0;
}

int ember_pubsub_subscribe(ember_pubsub_t *ps, const char *topic_prefix)
{
    if (!ps || ps->is_publisher || !topic_prefix) {
        return -EINVAL;
    }

    if (ps->default_subscribe_all) {
        (void)zmq_setsockopt(ps->sock, ZMQ_UNSUBSCRIBE, "", 0);
        ps->default_subscribe_all = 0;
    }

    size_t len = strlen(topic_prefix);
    if (zmq_setsockopt(ps->sock, ZMQ_SUBSCRIBE, topic_prefix, len) != 0) {
        return -1;
    }
    return 0;
}

int ember_pubsub_publish(ember_pubsub_t *ps, const char *topic, const void *payload,
                         size_t payload_len)
{
    if (!ps || !ps->is_publisher || !topic) {
        return -EINVAL;
    }

    size_t tlen = strlen(topic);
    if (zmq_send(ps->sock, topic, tlen, ZMQ_SNDMORE) < 0) {
        return -1;
    }
    if (zmq_send(ps->sock, payload, payload_len, 0) < 0) {
        return -1;
    }
    return 0;
}

void ember_pubsub_destroy(ember_pubsub_t *ps)
{
    if (!ps) {
        return;
    }

    if (!ps->is_publisher) {
        ps->recv_running = 0;
        (void)pthread_join(ps->recv_thread, NULL);
    }

    zmq_close(ps->sock);
    zmq_ctx_term(ps->ctx);
    free(ps);
}
