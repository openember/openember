/*
 * Copyright (c) 2025, OpenEmber Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * 内置 UDP 报文 pub/sub（无第三方依赖），用于演示与无 libzmq 环境。
 *
 * URL 格式: udp://<host>:<port>  例如 udp://127.0.0.1:7560
 * - 发布端: 向该地址 sendto
 * - 订阅端: 在本机绑定该 port 收包（INADDR_ANY）
 *
 * 单包格式: [topic_len: uint16 BE][payload_len: uint32 BE][topic][payload]
 */

#include "ember_pubsub.h"

#include <arpa/inet.h>
#include <errno.h>
#include <stdio.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define EMBER_PS_HDR_SIZE 6u
#define EMBER_PS_MAX_TOPIC 1024u
#define EMBER_PS_MAX_PAYLOAD 60000u
#define EMBER_PS_RX_BUF (EMBER_PS_HDR_SIZE + EMBER_PS_MAX_TOPIC + EMBER_PS_MAX_PAYLOAD)

struct ember_pubsub {
    int sock;
    int is_publisher;
    struct sockaddr_in send_dest;
    int has_send_dest;

    pthread_t recv_thread;
    volatile int recv_running;
    ember_pubsub_on_message_t on_message;
    void *user_data;

    char topic_filter[256];
    size_t filter_len;
};

static int parse_udp_url(const char *url, struct sockaddr_in *out)
{
    if (!url || strncmp(url, "udp://", 6) != 0) {
        return -EINVAL;
    }

    char host[256];
    unsigned port = 0;
    const char *rest = url + 6;

    if (sscanf(rest, "%255[^:]:%u", host, &port) < 1) {
        return -EINVAL;
    }
    if (port == 0u) {
        port = 7560u;
    }
    if (port > 65535u) {
        return -EINVAL;
    }

    memset(out, 0, sizeof(*out));
    out->sin_family = AF_INET;
    out->sin_port = htons((uint16_t)port);
    if (inet_pton(AF_INET, host, &out->sin_addr) != 1) {
        return -EINVAL;
    }
    return 0;
}

static void *recv_loop_udp(void *arg)
{
    ember_pubsub_t *ps = (ember_pubsub_t *)arg;
    unsigned char *buf = (unsigned char *)malloc(EMBER_PS_RX_BUF);
    if (!buf) {
        return NULL;
    }

    while (ps->recv_running) {
        struct sockaddr_in from;
        socklen_t fromlen = sizeof(from);
        ssize_t n = recvfrom(ps->sock, buf, EMBER_PS_RX_BUF, 0, (struct sockaddr *)&from, &fromlen);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            break;
        }
        if (n < (ssize_t)EMBER_PS_HDR_SIZE) {
            continue;
        }

        uint16_t tlen = (uint16_t)((buf[0] << 8) | buf[1]);
        uint32_t plen = ((uint32_t)buf[2] << 24) | ((uint32_t)buf[3] << 16) | ((uint32_t)buf[4] << 8) |
                        (uint32_t)buf[5];

        if (tlen > EMBER_PS_MAX_TOPIC || plen > EMBER_PS_MAX_PAYLOAD) {
            continue;
        }
        if ((ssize_t)(EMBER_PS_HDR_SIZE + tlen + plen) != n) {
            continue;
        }

        const char *topic_ptr = (const char *)(buf + EMBER_PS_HDR_SIZE);
        const unsigned char *payload_ptr = buf + EMBER_PS_HDR_SIZE + tlen;

        if (ps->filter_len > 0u) {
            if (tlen < ps->filter_len || memcmp(topic_ptr, ps->topic_filter, ps->filter_len) != 0) {
                continue;
            }
        }

        char *topic_copy = (char *)malloc((size_t)tlen + 1u);
        if (!topic_copy) {
            continue;
        }
        memcpy(topic_copy, topic_ptr, tlen);
        topic_copy[tlen] = '\0';

        void *payload_copy = malloc(plen ? plen : 1u);
        if (!payload_copy) {
            free(topic_copy);
            continue;
        }
        if (plen > 0u) {
            memcpy(payload_copy, payload_ptr, plen);
        }

        if (ps->on_message) {
            ps->on_message(topic_copy, payload_copy, (size_t)plen, ps->user_data);
        }
        free(topic_copy);
        free(payload_copy);
    }

    free(buf);
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

    if (parse_udp_url(bind_url, &ps->send_dest) != 0) {
        free(ps);
        return -EINVAL;
    }

    ps->sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (ps->sock < 0) {
        free(ps);
        return -1;
    }

    ps->is_publisher = 1;
    ps->has_send_dest = 1;
    *out = ps;
    return 0;
}

int ember_pubsub_create_subscriber(ember_pubsub_t **out, const char *connect_url,
                                   ember_pubsub_on_message_t on_message, void *user_data)
{
    if (!out || !connect_url) {
        return -EINVAL;
    }

    struct sockaddr_in target;
    if (parse_udp_url(connect_url, &target) != 0) {
        return -EINVAL;
    }

    ember_pubsub_t *ps = (ember_pubsub_t *)calloc(1, sizeof(*ps));
    if (!ps) {
        return -ENOMEM;
    }

    ps->on_message = on_message;
    ps->user_data = user_data;
    ps->filter_len = 0;

    ps->sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (ps->sock < 0) {
        free(ps);
        return -1;
    }

    int reuse = 1;
    if (setsockopt(ps->sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) != 0) {
        close(ps->sock);
        free(ps);
        return -1;
    }

    struct sockaddr_in bindaddr;
    memset(&bindaddr, 0, sizeof(bindaddr));
    bindaddr.sin_family = AF_INET;
    bindaddr.sin_port = target.sin_port;
    bindaddr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(ps->sock, (struct sockaddr *)&bindaddr, sizeof(bindaddr)) != 0) {
        close(ps->sock);
        free(ps);
        return -1;
    }

    ps->is_publisher = 0;
    ps->recv_running = 1;
    if (pthread_create(&ps->recv_thread, NULL, recv_loop_udp, ps) != 0) {
        ps->recv_running = 0;
        close(ps->sock);
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

    size_t n = strlen(topic_prefix);
    if (n >= sizeof(ps->topic_filter)) {
        return -EINVAL;
    }
    memcpy(ps->topic_filter, topic_prefix, n);
    ps->topic_filter[n] = '\0';
    ps->filter_len = n;
    return 0;
}

int ember_pubsub_publish(ember_pubsub_t *ps, const char *topic, const void *payload, size_t payload_len)
{
    if (!ps || !ps->is_publisher || !topic || !ps->has_send_dest) {
        return -EINVAL;
    }

    size_t tlen = strlen(topic);
    if (tlen > EMBER_PS_MAX_TOPIC || payload_len > EMBER_PS_MAX_PAYLOAD) {
        return -EMSGSIZE;
    }

    size_t total = EMBER_PS_HDR_SIZE + tlen + payload_len;
    unsigned char *buf = (unsigned char *)malloc(total);
    if (!buf) {
        return -ENOMEM;
    }

    buf[0] = (unsigned char)((tlen >> 8) & 0xff);
    buf[1] = (unsigned char)(tlen & 0xff);
    buf[2] = (unsigned char)((payload_len >> 24) & 0xff);
    buf[3] = (unsigned char)((payload_len >> 16) & 0xff);
    buf[4] = (unsigned char)((payload_len >> 8) & 0xff);
    buf[5] = (unsigned char)(payload_len & 0xff);
    memcpy(buf + EMBER_PS_HDR_SIZE, topic, tlen);
    memcpy(buf + EMBER_PS_HDR_SIZE + tlen, payload, payload_len);

    ssize_t sn = sendto(ps->sock, buf, total, 0, (struct sockaddr *)&ps->send_dest, sizeof(ps->send_dest));
    free(buf);

    if (sn < 0 || (size_t)sn != total) {
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

    close(ps->sock);
    free(ps);
}
