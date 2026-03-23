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

#include <nng/nng.h>

// NNG requires initialization before first use.
// Keep a per-process refcount so multiple instances in one process don't double-init/fini.
static pthread_mutex_t g_nng_ref_mtx = PTHREAD_MUTEX_INITIALIZER;
static int g_nng_ref_cnt = 0;

static int ember_nng_ensure_init(void)
{
    pthread_mutex_lock(&g_nng_ref_mtx);
    if (g_nng_ref_cnt == 0) {
        nng_init_params params = {0};
        int rv = nng_init(&params);
        if (rv != 0) {
            pthread_mutex_unlock(&g_nng_ref_mtx);
            return -rv;
        }
    }
    g_nng_ref_cnt++;
    pthread_mutex_unlock(&g_nng_ref_mtx);
    return 0;
}

static void ember_nng_release(void)
{
    pthread_mutex_lock(&g_nng_ref_mtx);
    if (g_nng_ref_cnt > 0) {
        g_nng_ref_cnt--;
        if (g_nng_ref_cnt == 0) {
            nng_fini();
        }
    }
    pthread_mutex_unlock(&g_nng_ref_mtx);
}

struct ember_pubsub {
    nng_socket sock;
    int is_publisher;

    pthread_t recv_thread;
    volatile int recv_running;

    ember_pubsub_on_message_t on_message;
    void *user_data;
};

static void parse_topic_payload(const void *msg, size_t msg_len,
                                  char **out_topic, size_t *out_topic_len,
                                  const void **out_payload,
                                  size_t *out_payload_len)
{
    const unsigned char *p = (const unsigned char *)msg;
    size_t i = 0;

    *out_topic = NULL;
    *out_topic_len = 0u;
    *out_payload = NULL;
    *out_payload_len = 0u;

    for (i = 0; i < msg_len; i++) {
        if (p[i] == 0x00) {
            *out_topic_len = i;
            *out_payload = (const unsigned char *)msg + i + 1u;
            *out_payload_len = (msg_len > i + 1u) ? (msg_len - (i + 1u)) : 0u;

            *out_topic = (char *)malloc(i + 1u);
            if (*out_topic) {
                memcpy(*out_topic, msg, i);
                (*out_topic)[i] = '\0';
            }
            return;
        }
    }

    // No delimiter: treat the whole message as topic, with empty payload.
    *out_topic_len = msg_len;
    *out_payload = NULL;
    *out_payload_len = 0u;

    *out_topic = (char *)malloc(msg_len + 1u);
    if (*out_topic) {
        memcpy(*out_topic, msg, msg_len);
        (*out_topic)[msg_len] = '\0';
    }
}

static void *recv_loop(void *arg)
{
    ember_pubsub_t *ps = (ember_pubsub_t *)arg;

    while (ps->recv_running) {
        nng_msg *msg = NULL;
        int rv = nng_recvmsg(ps->sock, &msg, 0);
        if (rv != 0) {
            // Socket probably closed during destroy; exit loop.
            if (!ps->recv_running) {
                break;
            }
            continue;
        }

        size_t msg_len = nng_msg_len(msg);
        void *msg_body = nng_msg_body(msg);

        char *topic_copy = NULL;
        size_t topic_len = 0u;
        const void *payload_ptr = NULL;
        size_t payload_len = 0u;
        (void)topic_len;

        parse_topic_payload(msg_body, msg_len, &topic_copy, &topic_len,
                             &payload_ptr, &payload_len);

        if (ps->on_message && topic_copy) {
            void *payload_copy = NULL;
            if (payload_len > 0u) {
                payload_copy = malloc(payload_len);
                if (payload_copy) {
                    memcpy(payload_copy, payload_ptr, payload_len);
                }
            }

            ps->on_message(topic_copy, payload_copy, payload_len, ps->user_data);

            free(payload_copy);
        }

        free(topic_copy);
        nng_msg_free(msg);
    }

    return NULL;
}

int ember_pubsub_create_publisher(ember_pubsub_t **out, const char *bind_url)
{
    if (!out || !bind_url) {
        return -EINVAL;
    }

    ember_pubsub_t *ps = (ember_pubsub_t *)calloc(1u, sizeof(*ps));
    if (!ps) {
        return -ENOMEM;
    }

    int init_rc = ember_nng_ensure_init();
    if (init_rc != 0) {
        free(ps);
        return init_rc;
    }

    int rv = nng_pub0_open(&ps->sock);
    if (rv != 0) {
        free(ps);
        return -rv;
    }

    rv = nng_listen(ps->sock, bind_url, NULL, 0);
    if (rv != 0) {
        nng_socket_close(ps->sock);
        free(ps);
        ember_nng_release();
        return -rv;
    }

    ps->is_publisher = 1;
    *out = ps;
    return 0;
}

int ember_pubsub_create_subscriber(ember_pubsub_t **out, const char *connect_url,
                                     ember_pubsub_on_message_t on_message, void *user_data)
{
    if (!out || !connect_url) {
        return -EINVAL;
    }

    ember_pubsub_t *ps = (ember_pubsub_t *)calloc(1u, sizeof(*ps));
    if (!ps) {
        return -ENOMEM;
    }

    int init_rc = ember_nng_ensure_init();
    if (init_rc != 0) {
        free(ps);
        return init_rc;
    }

    int rv = nng_sub0_open(&ps->sock);
    if (rv != 0) {
        free(ps);
        ember_nng_release();
        return -rv;
    }

    // Default: subscribe to empty prefix => receive all topics.
    rv = nng_sub0_socket_subscribe(ps->sock, "", 0u);
    if (rv != 0) {
        nng_socket_close(ps->sock);
        free(ps);
        ember_nng_release();
        return -rv;
    }

    rv = nng_dial(ps->sock, connect_url, NULL, 0);
    if (rv != 0) {
        nng_socket_close(ps->sock);
        free(ps);
        ember_nng_release();
        return -rv;
    }

    ps->is_publisher = 0;
    ps->on_message = on_message;
    ps->user_data = user_data;
    ps->recv_running = 1;

    if (pthread_create(&ps->recv_thread, NULL, recv_loop, ps) != 0) {
        ps->recv_running = 0;
        nng_socket_close(ps->sock);
        free(ps);
        ember_nng_release();
        return -EAGAIN;
    }

    *out = ps;
    return 0;
}

int ember_pubsub_subscribe(ember_pubsub_t *ps, const char *topic_prefix)
{
    if (!ps || !topic_prefix) {
        return -EINVAL;
    }

    size_t len = strlen(topic_prefix);
    int rv = nng_sub0_socket_subscribe(ps->sock, topic_prefix, len);
    return (rv == 0) ? 0 : -rv;
}

int ember_pubsub_publish(ember_pubsub_t *ps, const char *topic, const void *payload,
                           size_t payload_len)
{
    if (!ps || !topic) {
        return -EINVAL;
    }

    if (ps->is_publisher == 0) {
        return -EINVAL;
    }

    size_t topic_len = strlen(topic);
    nng_msg *msg = NULL;

    int rv = nng_msg_alloc(&msg, 0u);
    if (rv != 0) {
        return -rv;
    }

    rv = nng_msg_append(msg, topic, topic_len);
    if (rv == 0) {
        rv = nng_msg_append(msg, "\0", 1u);
    }
    if (rv == 0 && payload_len > 0u && payload) {
        rv = nng_msg_append(msg, payload, payload_len);
    }

    if (rv != 0) {
        nng_msg_free(msg);
        return -rv;
    }

    rv = nng_sendmsg(ps->sock, msg, 0);
    if (rv != 0) {
        // On error, ownership may still belong to caller for this wrapper;
        // be defensive and free.
        nng_msg_free(msg);
        return -rv;
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
        // Closing the socket should unblock recv loop quickly.
        nng_socket_close(ps->sock);
        pthread_join(ps->recv_thread, NULL);
    } else {
        nng_socket_close(ps->sock);
    }

    free(ps);
    ember_nng_release();
}

