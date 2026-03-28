/*
 * LCM based internal msgbus wrapper.
 *
 * This implementation publishes/subscribes to a single LCM channel and
 * filters topic prefixes in-process.
 */
#define LOG_TAG "MSG"
#include "openember.h"

#ifdef EMBER_LIBS_USING_LCM

#include <lcm/lcm.h>

#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lcm_wrapper.h"

#define EMBER_LCM_MSGBUS_CHANNEL "openember/msgbus"
#define EMBER_LCM_MSGBUS_DEFAULT_PROVIDER "udpm://239.255.76.67:7667"

#ifndef EMBER_LCM_MSGBUS_MAX_SUBS
#define EMBER_LCM_MSGBUS_MAX_SUBS 64
#endif

typedef struct msgbus_lcm {
    lcm_t *lcm;
    lcm_subscription_t *subscription;

    msg_arrived_cb_t *cb;
    void *user_data;
    pthread_t recv_thread;
    volatile int recv_running;

    pthread_mutex_t mtx; /* protects subscriptions and sync buffers */

    /* Prefix subscriptions list (in-process filtering). */
    char *subs[EMBER_LCM_MSGBUS_MAX_SUBS];
    size_t sub_count;

    /* Sync-mode receive buffer (owned by wrapper until consumed). */
    char *sync_topic;
    void *sync_payload;
    int sync_payload_len;
    int sync_has_msg;
} msgbus_lcm_t;

static int split_topic_payload(const void *buf, size_t len,
                                 char **out_topic, void **out_payload,
                                 size_t *out_payload_len)
{
    if (!buf || !out_topic || !out_payload || !out_payload_len) {
        return -EINVAL;
    }

    const unsigned char *p = (const unsigned char *)buf;
    size_t i = 0u;
    for (i = 0u; i < len; i++) {
        if (p[i] == 0x00u) {
            size_t topic_len = i;
            size_t payload_len = (len > (i + 1u)) ? (len - (i + 1u)) : 0u;

            char *topic_copy = (char *)malloc(topic_len + 1u);
            if (!topic_copy) {
                return -ENOMEM;
            }
            if (topic_len > 0u) {
                memcpy(topic_copy, buf, topic_len);
            }
            topic_copy[topic_len] = '\0';

            void *payload_copy = NULL;
            if (payload_len > 0u) {
                payload_copy = malloc(payload_len);
                if (!payload_copy) {
                    free(topic_copy);
                    return -ENOMEM;
                }
                memcpy(payload_copy, (const unsigned char *)buf + (i + 1u), payload_len);
            }

            *out_topic = topic_copy;
            *out_payload = payload_copy;
            *out_payload_len = payload_len;
            return 0;
        }
    }

    return -EINVAL; /* no delimiter */
}

static int topic_matches(msgbus_lcm_t *ps, const char *topic)
{
    if (!ps || !topic) {
        return 0;
    }
    if (ps->sub_count == 0u) {
        return 0;
    }

    for (size_t i = 0u; i < ps->sub_count; i++) {
        const char *sub = ps->subs[i];
        if (!sub) {
            continue;
        }
        size_t slen = strlen(sub);
        if (strncmp(topic, sub, slen) == 0) {
            return 1;
        }
    }
    return 0;
}

static void lcm_on_message(const lcm_recv_buf_t *rbuf, const char *channel, void *user_data)
{
    (void)channel;
    if (!rbuf || !user_data) {
        return;
    }

    msgbus_lcm_t *ps = (msgbus_lcm_t *)user_data;
    const void *data = rbuf->data;
    const size_t len = (size_t)rbuf->data_size;

    char *topic_copy = NULL;
    void *payload_copy = NULL;
    size_t payload_len = 0u;

    int rv = split_topic_payload(data, len, &topic_copy, &payload_copy, &payload_len);
    if (rv != 0) {
        if (topic_copy) {
            free(topic_copy);
        }
        if (payload_copy) {
            free(payload_copy);
        }
        return;
    }

    pthread_mutex_lock(&ps->mtx);
    int matched = topic_matches(ps, topic_copy);

    if (ps->cb && matched) {
        msg_arrived_cb_t *cb = ps->cb;
        void *ud = ps->user_data;
        pthread_mutex_unlock(&ps->mtx);

        cb(ud, topic_copy, payload_copy, payload_len);
        free(topic_copy);
        free(payload_copy);
        return;
    }

    if (!ps->cb && matched) {
        /* Overwrite previous sync message if not consumed yet. */
        if (ps->sync_topic) {
            free(ps->sync_topic);
            ps->sync_topic = NULL;
        }
        if (ps->sync_payload) {
            free(ps->sync_payload);
            ps->sync_payload = NULL;
        }

        ps->sync_topic = topic_copy; /* ownership transferred to msg_bus_recv */
        ps->sync_payload = payload_copy;
        ps->sync_payload_len = (int)payload_len;
        ps->sync_has_msg = 1;

        pthread_mutex_unlock(&ps->mtx);
        return;
    }

    pthread_mutex_unlock(&ps->mtx);

    /* Not matched (or sync/async mismatch): free locally. */
    free(topic_copy);
    free(payload_copy);
}

static void *lcm_recv_loop(void *arg)
{
    msgbus_lcm_t *ps = (msgbus_lcm_t *)arg;
    if (!ps) {
        return NULL;
    }

    while (ps->recv_running) {
        /* Dispatch at a bounded rate so deinit can stop promptly. */
        (void)lcm_handle_timeout(ps->lcm, 200);
    }
    return NULL;
}

int msg_bus_init(msg_node_t *handle, const char *name, char *address, msg_arrived_cb_t *cb,
                 void *user_data)
{
    (void)name;

    if (!handle) {
        return -EINVAL;
    }

    msgbus_lcm_t *ps = (msgbus_lcm_t *)calloc(1u, sizeof(*ps));
    if (!ps) {
        return -ENOMEM;
    }

    const char *provider = address ? address : EMBER_LCM_MSGBUS_DEFAULT_PROVIDER;

    ps->lcm = lcm_create(provider);
    if (!ps->lcm) {
        free(ps);
        return -EMBER_ERROR;
    }

    ps->cb = cb;
    ps->user_data = user_data;
    ps->recv_running = (cb != NULL) ? 1 : 0;
    ps->sync_topic = NULL;
    ps->sync_payload = NULL;
    ps->sync_payload_len = 0;
    ps->sync_has_msg = 0;
    ps->sub_count = 0u;

    pthread_mutex_init(&ps->mtx, NULL);

    ps->subscription = lcm_subscribe(ps->lcm, EMBER_LCM_MSGBUS_CHANNEL, lcm_on_message, ps);
    if (!ps->subscription) {
        lcm_destroy(ps->lcm);
        pthread_mutex_destroy(&ps->mtx);
        free(ps);
        return -EMBER_ERROR;
    }

    if (cb) {
        if (pthread_create(&ps->recv_thread, NULL, lcm_recv_loop, ps) != 0) {
            ps->recv_running = 0;
            lcm_unsubscribe(ps->lcm, ps->subscription);
            lcm_destroy(ps->lcm);
            pthread_mutex_destroy(&ps->mtx);
            free(ps);
            return -EAGAIN;
        }
    }

    *handle = (msg_node_t)ps;
    return EMBER_EOK;
}

int msg_bus_deinit(msg_node_t handle)
{
    if (!handle) {
        return -EINVAL;
    }

    msgbus_lcm_t *ps = (msgbus_lcm_t *)handle;
    ps->recv_running = 0;

    if (ps->cb) {
        pthread_join(ps->recv_thread, NULL);
    }

    if (ps->subscription) {
        (void)lcm_unsubscribe(ps->lcm, ps->subscription);
    }
    if (ps->lcm) {
        lcm_destroy(ps->lcm);
    }

    for (size_t i = 0u; i < ps->sub_count; i++) {
        if (ps->subs[i]) {
            free(ps->subs[i]);
            ps->subs[i] = NULL;
        }
    }

    if (ps->sync_topic) {
        free(ps->sync_topic);
        ps->sync_topic = NULL;
    }
    if (ps->sync_payload) {
        free(ps->sync_payload);
        ps->sync_payload = NULL;
    }

    pthread_mutex_destroy(&ps->mtx);
    free(ps);
    return EMBER_EOK;
}

int msg_bus_publish(msg_node_t handle, const char *topic, const char *payload)
{
    if (!handle || !topic || !payload) {
        return -EINVAL;
    }
    return msg_bus_publish_raw(handle, topic, payload, (int)strlen(payload));
}

int msg_bus_publish_raw(msg_node_t handle, const char *topic, const void *payload, const int payloadlen)
{
    if (!handle || !topic || !payload) {
        return -EINVAL;
    }
    if (payloadlen < 0) {
        return -EINVAL;
    }

    size_t tlen = strlen(topic);
    size_t plen = (payloadlen > 0) ? (size_t)payloadlen : 0u;
    size_t msg_len = tlen + 1u + plen;

    unsigned char *msg = (unsigned char *)malloc(msg_len);
    if (!msg) {
        return -ENOMEM;
    }

    if (tlen > 0u) {
        memcpy(msg, topic, tlen);
    }
    msg[tlen] = 0x00u;
    if (plen > 0u) {
        memcpy(msg + (tlen + 1u), payload, plen);
    }

    int rv = lcm_publish(((msgbus_lcm_t *)handle)->lcm, EMBER_LCM_MSGBUS_CHANNEL, msg, (unsigned int)msg_len);
    free(msg);

    return (rv == 0) ? EMBER_EOK : -EMBER_ERROR;
}

int msg_bus_subscribe(msg_node_t handle, const char *topic)
{
    if (!handle || !topic) {
        return -EINVAL;
    }

    msgbus_lcm_t *ps = (msgbus_lcm_t *)handle;
    pthread_mutex_lock(&ps->mtx);

    /* De-dup. */
    for (size_t i = 0u; i < ps->sub_count; i++) {
        if (ps->subs[i] && strcmp(ps->subs[i], topic) == 0) {
            pthread_mutex_unlock(&ps->mtx);
            return EMBER_EOK;
        }
    }

    if (ps->sub_count >= EMBER_LCM_MSGBUS_MAX_SUBS) {
        pthread_mutex_unlock(&ps->mtx);
        return -ENOMEM;
    }

    ps->subs[ps->sub_count] = strdup(topic);
    if (!ps->subs[ps->sub_count]) {
        pthread_mutex_unlock(&ps->mtx);
        return -ENOMEM;
    }
    ps->sub_count++;

    pthread_mutex_unlock(&ps->mtx);
    return EMBER_EOK;
}

int msg_bus_unsubscribe(msg_node_t handle, const char *topic)
{
    if (!handle || !topic) {
        return -EINVAL;
    }

    msgbus_lcm_t *ps = (msgbus_lcm_t *)handle;
    pthread_mutex_lock(&ps->mtx);

    for (size_t i = 0u; i < ps->sub_count; i++) {
        if (ps->subs[i] && strcmp(ps->subs[i], topic) == 0) {
            free(ps->subs[i]);
            ps->subs[i] = NULL;

            /* compact array */
            for (size_t j = i + 1u; j < ps->sub_count; j++) {
                ps->subs[j - 1u] = ps->subs[j];
                ps->subs[j] = NULL;
            }
            ps->sub_count--;
            pthread_mutex_unlock(&ps->mtx);
            return EMBER_EOK;
        }
    }

    pthread_mutex_unlock(&ps->mtx);
    return EMBER_EOK;
}

int msg_bus_recv(msg_node_t handle, char **topicName, void **payload, int *payloadLen, time_t timeout)
{
    if (!handle || !topicName || !payload || !payloadLen) {
        return -EINVAL;
    }

    msgbus_lcm_t *ps = (msgbus_lcm_t *)handle;
    if (ps->cb) {
        /* Async mode is active; sync recv isn't supported. */
        return -EMBER_ERROR;
    }

    int timeout_ms = (int)timeout;
    if (timeout_ms < 0) {
        timeout_ms = 0;
    }

    pthread_mutex_lock(&ps->mtx);
    if (ps->sync_topic) {
        free(ps->sync_topic);
        ps->sync_topic = NULL;
    }
    if (ps->sync_payload) {
        free(ps->sync_payload);
        ps->sync_payload = NULL;
    }
    ps->sync_payload_len = 0;
    ps->sync_has_msg = 0;
    pthread_mutex_unlock(&ps->mtx);

    int remaining = timeout_ms;
    const int step_ms = 200;

    while (remaining > 0) {
        int step = (remaining > step_ms) ? step_ms : remaining;
        (void)lcm_handle_timeout(ps->lcm, step);

        pthread_mutex_lock(&ps->mtx);
        int got = ps->sync_has_msg;
        pthread_mutex_unlock(&ps->mtx);

        if (got) {
            pthread_mutex_lock(&ps->mtx);
            *topicName = ps->sync_topic;
            *payload = ps->sync_payload;
            *payloadLen = ps->sync_payload_len;

            ps->sync_topic = NULL;
            ps->sync_payload = NULL;
            ps->sync_payload_len = 0;
            ps->sync_has_msg = 0;
            pthread_mutex_unlock(&ps->mtx);

            return EMBER_EOK;
        }

        remaining -= step;
    }

    return -EMBER_ETIMEOUT;
}

void msg_bus_free(void *topic, void *payload)
{
    if (topic) {
        free(topic);
    }
    if (payload) {
        free(payload);
    }
}

#endif /* EMBER_LIBS_USING_LCM */

