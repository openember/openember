/*
 * NNG based internal msgbus wrapper.
 *
 * This implementation uses an internal bridge forwarder based on NNG:
 * - Module publishers LISTEN on IN_URL using PUB (forwarder SUB dials)
 * - Module subscribes by dialing OUT_URL using SUB (forwarder PUB listens)
 *
 * IN_URL and OUT_URL default to:
 *   OUT: tcp://127.0.0.1:5560
 *   IN : tcp://127.0.0.1:5561
 *
 * When address is provided to msg_bus_init(), it is treated as OUT_URL,
 * and IN_URL is derived as (port + 1).
 */

#define LOG_TAG "MSG"
#include "openember.h"

#ifdef EMBER_LIBS_USING_NNG

#include <errno.h>
#include <stdio.h>
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <nng/nng.h>

#include "nng_wrapper.h"

// NNG requires initialization before first use.
// Keep a per-process refcount so multiple instances in one process don't double-init/fini.
static pthread_mutex_t g_nng_ref_mtx = PTHREAD_MUTEX_INITIALIZER;
static int g_nng_ref_cnt = 0;

static int msgbus_nng_ensure_init(void)
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

static void msgbus_nng_release(void)
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

#ifndef NNG_MSGBUS_DEFAULT_OUT
#define NNG_MSGBUS_DEFAULT_OUT "tcp://127.0.0.1:5560"
#endif
#ifndef NNG_MSGBUS_DEFAULT_IN
#define NNG_MSGBUS_DEFAULT_IN "tcp://127.0.0.1:5561"
#endif

typedef struct msgbus_nng {
    nng_socket pub_sock;
    nng_socket sub_sock;

    pthread_t recv_thread;
    volatile int recv_running;

    msg_arrived_cb_t *cb;
} msgbus_nng_t;

static int split_topic_payload(const void *buf, size_t len,
                                 char **out_topic, void **out_payload,
                                 size_t *out_payload_len)
{
    const unsigned char *p = (const unsigned char *)buf;
    size_t i = 0u;

    *out_topic = NULL;
    *out_payload = NULL;
    *out_payload_len = 0u;

    for (i = 0u; i < len; i++) {
        if (p[i] == 0x00u) {
            size_t topic_len = i;
            size_t payload_len = (len > (i + 1u)) ? (len - (i + 1u)) : 0u;

            char *topic_copy = (char *)malloc(topic_len + 1u);
            if (!topic_copy) {
                return -ENOMEM;
            }
            memcpy(topic_copy, buf, topic_len);
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

    // No delimiter: treat the whole message as topic.
    char *topic_copy = (char *)malloc(len + 1u);
    if (!topic_copy) {
        return -ENOMEM;
    }
    memcpy(topic_copy, buf, len);
    topic_copy[len] = '\0';

    *out_topic = topic_copy;
    *out_payload = NULL;
    *out_payload_len = 0u;
    return 0;
}

static int derive_in_url(const char *out_url, char *in_url, size_t in_url_sz)
{
    if (!out_url || !in_url || in_url_sz == 0u) {
        return -EINVAL;
    }

    // Very simple tcp://host:port parser (no ipv6/wildcards handling).
    const char *prefix = "tcp://";
    if (strncmp(out_url, prefix, strlen(prefix)) != 0) {
        // Fallback to defaults for non-tcp schemes (e.g. ipc://).
        strncpy(in_url, NNG_MSGBUS_DEFAULT_IN, in_url_sz - 1u);
        in_url[in_url_sz - 1u] = '\0';
        return 0;
    }

    char host[256];
    unsigned port = 0u;
    const char *rest = out_url + strlen(prefix);
    if (sscanf(rest, "%255[^:]:%u", host, &port) < 1) {
        strncpy(in_url, NNG_MSGBUS_DEFAULT_IN, in_url_sz - 1u);
        in_url[in_url_sz - 1u] = '\0';
        return -EINVAL;
    }

    unsigned in_port = port + 1u;
    // For listeners, using wildcard address is more compatible across environments.
    if (strcmp(host, "127.0.0.1") == 0 || strcmp(host, "localhost") == 0) {
        snprintf(in_url, in_url_sz, "tcp://:%u", in_port);
    }
    else {
        snprintf(in_url, in_url_sz, "tcp://%s:%u", host, in_port);
    }
    return 0;
}

static int encode_topic_payload(nng_msg **msgp, const char *topic, const void *payload, size_t payload_len)
{
    if (!msgp || !topic) {
        return -EINVAL;
    }

    size_t topic_len = strlen(topic);

    int rv = nng_msg_alloc(msgp, 0u);
    if (rv != 0) {
        return rv;
    }

    rv = nng_msg_append(*msgp, topic, topic_len);
    if (rv != 0) {
        nng_msg_free(*msgp);
        return rv;
    }

    unsigned char z = 0x00u;
    rv = nng_msg_append(*msgp, &z, 1u);
    if (rv != 0) {
        nng_msg_free(*msgp);
        return rv;
    }

    if (payload_len > 0u && payload) {
        rv = nng_msg_append(*msgp, payload, payload_len);
        if (rv != 0) {
            nng_msg_free(*msgp);
            return rv;
        }
    }

    return 0;
}

static void *recv_loop(void *arg)
{
    msgbus_nng_t *ps = (msgbus_nng_t *)arg;

    while (ps->recv_running) {
        nng_msg *msg = NULL;
        int rv = nng_recvmsg(ps->sub_sock, &msg, 0);
        if (rv != 0) {
            if (!ps->recv_running) {
                break;
            }
            continue;
        }

        size_t msg_len = nng_msg_len(msg);
        void *body = nng_msg_body(msg);

        char *topic_copy = NULL;
        void *payload_copy = NULL;
        size_t payload_len = 0u;

        (void)split_topic_payload(body, msg_len, &topic_copy, &payload_copy, &payload_len);

        if (ps->cb && topic_copy) {
            ps->cb(topic_copy, payload_copy, payload_len);
        }

        if (payload_copy) {
            free(payload_copy);
        }
        if (topic_copy) {
            free(topic_copy);
        }

        nng_msg_free(msg);
    }

    return NULL;
}

int msg_bus_init(msg_node_t *handle, const char *name, char *address, msg_arrived_cb_t *cb)
{
    (void)name;

    if (!handle) {
        return -EINVAL;
    }

    const char *out_url = address ? address : NNG_MSGBUS_DEFAULT_OUT;

    char in_url[256];
    memset(in_url, 0, sizeof(in_url));
    derive_in_url(out_url, in_url, sizeof(in_url));

    msgbus_nng_t *ps = (msgbus_nng_t *)calloc(1u, sizeof(*ps));
    if (!ps) {
        return -ENOMEM;
    }

    int init_rc = msgbus_nng_ensure_init();
    if (init_rc != 0) {
        free(ps);
        return init_rc;
    }

    int rv = nng_pub0_open(&ps->pub_sock);
    if (rv != 0) {
        fprintf(stderr, "msgbus_nng: nng_pub0_open failed: %d\n", rv);
        free(ps);
        msgbus_nng_release();
        return -rv;
    }

    // PUB side listens; forwarder SUB dials.
    if (strncmp(in_url, "ipc://", 6) == 0) {
        // NNG will create an ipc socket file; remove stale one first.
        (void)unlink(in_url + 6);
    }
    rv = nng_listen(ps->pub_sock, in_url, NULL, 0);
    if (rv != 0) {
        fprintf(stderr, "msgbus_nng: nng_listen(pub) failed: %d url=%s\n", rv, in_url);
        nng_socket_close(ps->pub_sock);
        free(ps);
        msgbus_nng_release();
        return -rv;
    }

    rv = nng_sub0_open(&ps->sub_sock);
    if (rv != 0) {
        fprintf(stderr, "msgbus_nng: nng_sub0_open failed: %d\n", rv);
        nng_socket_close(ps->pub_sock);
        free(ps);
        msgbus_nng_release();
        return -rv;
    }

    // Subscribe to all initially; actual topics are refined later.
    rv = nng_sub0_socket_subscribe(ps->sub_sock, "", 0u);
    if (rv != 0) {
        fprintf(stderr, "msgbus_nng: nng_sub0_socket_subscribe failed: %d\n", rv);
        nng_socket_close(ps->pub_sock);
        nng_socket_close(ps->sub_sock);
        free(ps);
        msgbus_nng_release();
        return -rv;
    }

    rv = nng_dial(ps->sub_sock, out_url, NULL, NNG_FLAG_NONBLOCK);
    if (rv != 0) {
        fprintf(stderr, "msgbus_nng: nng_dial(sub) failed: %d url=%s\n", rv, out_url);
        nng_socket_close(ps->pub_sock);
        nng_socket_close(ps->sub_sock);
        free(ps);
        msgbus_nng_release();
        return -rv;
    }

    ps->cb = cb;
    ps->recv_running = (cb != NULL) ? 1 : 0;

    if (cb) {
        if (pthread_create(&ps->recv_thread, NULL, recv_loop, ps) != 0) {
            ps->recv_running = 0;
            nng_socket_close(ps->sub_sock);
            nng_socket_close(ps->pub_sock);
            free(ps);
            msgbus_nng_release();
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

    msgbus_nng_t *ps = (msgbus_nng_t *)handle;

    ps->recv_running = 0;
    if (ps->cb) {
        // Closing sockets should unblock recv loop quickly.
        nng_socket_close(ps->sub_sock);
        pthread_join(ps->recv_thread, NULL);
    }

    nng_socket_close(ps->pub_sock);
    nng_socket_close(ps->sub_sock);
    free(ps);
    msgbus_nng_release();
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
    if (!handle || !topic) {
        return -EINVAL;
    }
    size_t plen = (payloadlen > 0) ? (size_t)payloadlen : 0u;

    nng_msg *msg = NULL;
    int rv = encode_topic_payload(&msg, topic, payload, plen);
    if (rv != 0) {
        return -rv;
    }

    rv = nng_sendmsg(((msgbus_nng_t *)handle)->pub_sock, msg, 0);
    if (rv != 0) {
        // msg might not be owned on error.
        nng_msg_free(msg);
        return -rv;
    }

    return EMBER_EOK;
}

int msg_bus_subscribe(msg_node_t handle, const char *topic)
{
    if (!handle || !topic) {
        return -EINVAL;
    }
    msgbus_nng_t *ps = (msgbus_nng_t *)handle;
    size_t len = strlen(topic);
    int rv = nng_sub0_socket_subscribe(ps->sub_sock, topic, len);
    return (rv == 0) ? EMBER_EOK : -rv;
}

int msg_bus_unsubscribe(msg_node_t handle, const char *topic)
{
    if (!handle || !topic) {
        return -EINVAL;
    }
    msgbus_nng_t *ps = (msgbus_nng_t *)handle;
    size_t len = strlen(topic);
    int rv = nng_sub0_socket_unsubscribe(ps->sub_sock, topic, len);
    return (rv == 0) ? EMBER_EOK : -rv;
}

int msg_bus_recv(msg_node_t handle, char **topicName, void **payload, int *payloadLen, time_t timeout)
{
    if (!handle || !topicName || !payload || !payloadLen) {
        return -EINVAL;
    }

    msgbus_nng_t *ps = (msgbus_nng_t *)handle;
    if (ps->cb) {
        // Async mode is active; sync recv isn't supported.
        return -EMBER_ERROR;
    }

    int rv = nng_socket_set_ms(ps->sub_sock, NNG_OPT_RECVTIMEO, (nng_duration)timeout);
    (void)rv;

    nng_msg *msg = NULL;
    rv = nng_recvmsg(ps->sub_sock, &msg, 0);
    if (rv != 0) {
        if (rv == NNG_ETIMEDOUT) {
            return -EMBER_ETIMEOUT;
        }
        return -EMBER_ERROR;
    }

    size_t msg_len = nng_msg_len(msg);
    void *body = nng_msg_body(msg);

    char *topic_copy = NULL;
    void *payload_copy = NULL;
    size_t payload_len = 0u;

    rv = split_topic_payload(body, msg_len, &topic_copy, &payload_copy, &payload_len);
    if (rv != 0) {
        nng_msg_free(msg);
        return -EMBER_ERROR;
    }

    *topicName = topic_copy;
    *payload = payload_copy;
    *payloadLen = (payload_len > 0u) ? (int)payload_len : 0;

    nng_msg_free(msg);
    return EMBER_EOK;
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

#endif /* EMBER_LIBS_USING_NNG */

