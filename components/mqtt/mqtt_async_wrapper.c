/*
 * Copyright (c) 2022-2023, OpenEmber Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2022-07-07     luhuadong    the first version
 */

#define LOG_TAG "MSG"
#include "openember.h"
#if defined (EMBER_LIBS_USING_MQTT) && defined (EMBER_LIBS_USING_MQTT_ASYNC) && ! defined(EMBER_LIBS_USING_MQTT_CLIENT)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "MQTTAsync.h"
#include "MQTTClient.h"
#include "mqtt_wrapper.h"

#define ADDRESS     "tcp://localhost:1883"
#define QOS         1
#define TIMEOUT     10000L

#ifdef __cplusplus
extern "C" {
#endif

static const char *username = "openember";
static const char *password = "p@ssw0rd";

typedef struct mqtt_handle_ctx {
    void *client;
    mqtt_cb_ctx_t *cb_ctx;
    struct mqtt_handle_ctx *next;
} mqtt_handle_ctx_t;

static mqtt_handle_ctx_t *g_handle_ctxs;

static mqtt_handle_ctx_t *mqtt_lookup_entry(void *client)
{
    for (mqtt_handle_ctx_t *p = g_handle_ctxs; p; p = p->next) {
        if (p->client == client) {
            return p;
        }
    }
    return NULL;
}

static int mqtt_register(void *client, mqtt_cb_ctx_t *cb_ctx)
{
    mqtt_handle_ctx_t *n = (mqtt_handle_ctx_t *)malloc(sizeof(*n));
    if (!n) {
        return -ENOMEM;
    }
    n->client = client;
    n->cb_ctx = cb_ctx;
    n->next = g_handle_ctxs;
    g_handle_ctxs = n;
    return 0;
}

static mqtt_cb_ctx_t *mqtt_unregister(void *client)
{
    mqtt_handle_ctx_t **pp = &g_handle_ctxs;
    while (*pp) {
        if ((*pp)->client == client) {
            mqtt_handle_ctx_t *dead = *pp;
            mqtt_cb_ctx_t *ctx = dead->cb_ctx;
            *pp = dead->next;
            free(dead);
            return ctx;
        }
        pp = &(*pp)->next;
    }
    return NULL;
}

#ifdef EMBER_LIBS_USING_MQTT_ASYNC
static void delivered(void *context, MQTTAsync_token token)
{
    //printf("\nMessage with token value %d delivery confirmed\n", token);
}

static int msgarrvd(void *context, char *topicName, int topicLen, MQTTAsync_message *message)
{
    mqtt_cb_ctx_t *ctx = (mqtt_cb_ctx_t *)context;

    //printf("\nMessage arrived\n");
    //printf("     topic: %s\n", topicName);
    //printf("   message: %.*s\n", message->payloadlen, (char*)message->payload);

    if (ctx && ctx->fn) {
        ctx->fn(ctx->user_data, topicName, message->payload, (size_t)message->payloadlen);
    }

    MQTTAsync_freeMessage(&message);
    MQTTAsync_free(topicName);
    return 1;
}

static void connlost(void *context, char *cause)
{
    printf("\nConnection lost\n");
    printf("     cause: %s\n", cause);
}
#else
static void delivered(void *context, MQTTClient_deliveryToken token)
{
    //printf("\nMessage with token value %d delivery confirmed\n", token);
}

static int msgarrvd(void *context, char *topicName, int topicLen, MQTTClient_message *message)
{
    mqtt_cb_ctx_t *ctx = (mqtt_cb_ctx_t *)context;

    //printf("\nMessage arrived\n");
    //printf("     topic: %s\n", topicName);
    //printf("   message: %.*s\n", message->payloadlen, (char*)message->payload);

    if (ctx && ctx->fn) {
        ctx->fn(ctx->user_data, topicName, message->payload, (size_t)message->payloadlen);
    }

    MQTTClient_freeMessage(&message);
    MQTTClient_free(topicName);
    return 1;
}

static void connlost(void *context, char *cause)
{
    printf("\nConnection lost\n");
    printf("     cause: %s\n", cause);
}
#endif

int msg_bus_init(msg_node_t *handle, const char *name, char *address, msg_arrived_cb_t *cb,
                 void *user_data)
{
    int rc;

#ifdef EMBER_LIBS_USING_MQTT_ASYNC
    MQTTAsync_createOptions create_opts = MQTTAsync_createOptions_initializer;
    MQTTAsync_connectOptions conn_opts = MQTTAsync_connectOptions_initializer;

    MQTTProperty property;
    MQTTProperties props = MQTTProperties_initializer;

    conn_opts.keepAliveInterval = 20;
	conn_opts.username = username;
	conn_opts.password = password;
	conn_opts.MQTTVersion = MQTTVERSION_5;
    //conn_opts.context  = (void*)mqtt_info;
    property.identifier = MQTTPROPERTY_CODE_SESSION_EXPIRY_INTERVAL;
	property.value.integer4 = 30;
	MQTTProperties_add(&props, &property);
    conn_opts.connectProperties = &props;

    rc = MQTTAsync_createWithOptions(handle, ADDRESS, name, MQTTCLIENT_PERSISTENCE_NONE, NULL, &create_opts);
    if (rc != MQTTASYNC_SUCCESS) {
        LOG_E("Failed to create client, return code %d\n", rc);
        return -EMBER_ERROR;
    }

    mqtt_cb_ctx_t *actx = NULL;
    if (cb) {
        actx = (mqtt_cb_ctx_t *)calloc(1, sizeof(*actx));
        if (!actx) {
            MQTTAsync_destroy(handle);
            return -ENOMEM;
        }
        actx->fn = cb;
        actx->user_data = user_data;
    }

    rc = MQTTAsync_setCallbacks(*handle, actx, connlost, msgarrvd, delivered);
    if (rc != MQTTASYNC_SUCCESS) {
        LOG_E("Failed to set callbacks, return code %d\n", rc);
        if (actx) {
            free(actx);
        }
        MQTTAsync_destroy(handle);
        return -EMBER_ERROR;
    }

    rc = MQTTAsync_connect(*handle, &conn_opts);
    if (rc != MQTTASYNC_SUCCESS) {
        LOG_E("Failed to connect, return code %d\n", rc);
        LOG_E("Reason: %s\n", MQTTReasonCode_toString(rc));
        if (actx) {
            free(actx);
        }
        MQTTAsync_destroy(handle);
        return -EMBER_ERROR;
    }

    if (mqtt_register(*handle, actx) != 0) {
        if (actx) {
            free(actx);
        }
        MQTTAsync_destroy(handle);
        return -ENOMEM;
    }

#else
    // 初始化 MQTT Client 选项
    MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;
    conn_opts.keepAliveInterval = 20;
    conn_opts.cleansession = 1;
    conn_opts.username = username;
    conn_opts.password = password;

    //#define MQTTClient_message_initializer { {'M', 'Q', 'T', 'M'}, 0, 0, NULL, 0, 0, 0, 0 }
    MQTTClient_message pubmsg = MQTTClient_message_initializer;
    // 声明消息 token
    MQTTClient_deliveryToken token;

    if (!address) address = ADDRESS;

    rc = MQTTClient_create(handle, address, name, MQTTCLIENT_PERSISTENCE_NONE, NULL);
    if (rc != MQTTCLIENT_SUCCESS) {
        LOG_E("Failed to create client, return code %d\n", rc);
        return -EMBER_ERROR;
    }

    mqtt_cb_ctx_t *sctx = NULL;
    if (cb) {
        sctx = (mqtt_cb_ctx_t *)calloc(1, sizeof(*sctx));
        if (!sctx) {
            MQTTClient_destroy(handle);
            return -ENOMEM;
        }
        sctx->fn = cb;
        sctx->user_data = user_data;
    }

    rc = MQTTClient_setCallbacks(*handle, sctx, connlost, msgarrvd, delivered);
    if (rc != MQTTCLIENT_SUCCESS) {
        LOG_E("Failed to set callbacks, return code %d\n", rc);
        if (sctx) {
            free(sctx);
        }
        MQTTClient_destroy(handle);
        return -EMBER_ERROR;
    }

    //使用MQTTClient_connect将client连接到服务器，使用指定的连接选项。成功则返回MQTTCLIENT_SUCCESS
    rc = MQTTClient_connect(*handle, &conn_opts);
    if (rc != MQTTCLIENT_SUCCESS) {
        LOG_E("Failed to connect, return code %d\n", rc);
        if (sctx) {
            free(sctx);
        }
        MQTTClient_destroy(handle);
        return -EMBER_ERROR;
    }

    if (mqtt_register(*handle, sctx) != 0) {
        if (sctx) {
            free(sctx);
        }
        MQTTClient_destroy(handle);
        return -ENOMEM;
    }
#endif

    return EMBER_EOK;
}

int msg_bus_deinit(msg_node_t handle)
{
    mqtt_cb_ctx_t *ctx = mqtt_unregister(handle);
    if (ctx) {
        free(ctx);
    }
#ifdef EMBER_LIBS_USING_MQTT_ASYNC
    MQTTAsync_destroy(handle);
#else
    MQTTClient_destroy(&handle);
#endif
    return EMBER_EOK;
}

int msg_bus_publish(msg_node_t handle, const char *topic, const char *payload)
{
    int rc;
    MQTTClient_deliveryToken token;
    MQTTClient_message pubmsg = MQTTClient_message_initializer;

    pubmsg.payload = (void *)payload;
    pubmsg.payloadlen = strlen(payload);
    pubmsg.qos = QOS;
    pubmsg.retained = 0;

    MQTTClient_publishMessage(handle, topic, &pubmsg, &token);
    rc = MQTTClient_waitForCompletion(handle, token, TIMEOUT);
    if (rc != MQTTCLIENT_SUCCESS) {
        LOG_E("Failed to publish, return code %d\n", rc);
        return -EMBER_ERROR;
    }

    return EMBER_EOK;
}

int msg_bus_publish_raw(msg_node_t handle, const char *topic, const void *payload, const int payloadlen)
{
    int rc;
    MQTTClient_deliveryToken token;

    MQTTClient_publish(handle, topic, payloadlen, payload, QOS, 0, &token);
    rc = MQTTClient_waitForCompletion(handle, token, TIMEOUT);
    if (rc != MQTTCLIENT_SUCCESS) {
        LOG_E("Failed to publish, return code %d\n", rc);
        return -EMBER_ERROR;
    }

    return EMBER_EOK;
}

int msg_bus_subscribe(msg_node_t handle, const char *topic)
{
    int rc;

#ifdef EMBER_LIBS_USING_MQTT_ASYNC
    MQTTAsync_responseOptions response = MQTTAsync_responseOptions_initializer;
    rc = MQTTAsync_subscribe(handle, topic, QOS, &response);
    if (rc != MQTTASYNC_SUCCESS) {
        LOG_E("Failed to subscribe, return code %d\n", rc);
        return -EMBER_ERROR;
    }
#else
    rc = MQTTClient_subscribe(handle, topic, QOS);
    if (rc != MQTTCLIENT_SUCCESS) {
        LOG_E("Failed to subscribe, return code %d\n", rc);
        return -EMBER_ERROR;
    }
#endif

    return EMBER_EOK;
}

int msg_bus_unsubscribe(msg_node_t handle, const char *topic)
{
    int rc;
    rc = MQTTClient_unsubscribe(handle, topic);
    if (rc != MQTTCLIENT_SUCCESS) {
        LOG_E("Failed to unsubscribe, return code %d\n", rc);
        return -EMBER_ERROR;
    }
    return EMBER_EOK;
}

int msg_bus_set_callback(msg_node_t handle, msg_arrived_cb_t *cb, void *user_data)
{
    int rc;
    mqtt_handle_ctx_t *entry = mqtt_lookup_entry(handle);
    mqtt_cb_ctx_t *ctx = NULL;

    if (!entry) {
        ctx = (mqtt_cb_ctx_t *)calloc(1, sizeof(*ctx));
        if (!ctx) {
            return -ENOMEM;
        }
        ctx->fn = cb;
        ctx->user_data = user_data;
        if (mqtt_register(handle, ctx) != 0) {
            free(ctx);
            return -ENOMEM;
        }
    } else {
        if (!entry->cb_ctx) {
            entry->cb_ctx = (mqtt_cb_ctx_t *)calloc(1, sizeof(mqtt_cb_ctx_t));
            if (!entry->cb_ctx) {
                return -ENOMEM;
            }
        }
        ctx = entry->cb_ctx;
        ctx->fn = cb;
        ctx->user_data = user_data;
    }

#ifdef EMBER_LIBS_USING_MQTT_ASYNC
    rc = MQTTAsync_setCallbacks(handle, ctx, connlost, msgarrvd, delivered);
    if (rc != MQTTASYNC_SUCCESS) {
        LOG_E("Failed to set callbacks, return code %d\n", rc);
        return -EMBER_ERROR;
    }
#else
    rc = MQTTClient_setCallbacks(handle, ctx, connlost, msgarrvd, delivered);
    if (rc != MQTTCLIENT_SUCCESS) {
        LOG_E("Failed to set callbacks, return code %d\n", rc);
        return -EMBER_ERROR;
    }
#endif
    return EMBER_EOK;
}

int msg_bus_connect(msg_node_t handle)
{
    int rc;

    // 初始化 MQTT Client 选项
    MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;
    conn_opts.keepAliveInterval = 20;
    conn_opts.cleansession = 1;
    conn_opts.username = username;
    conn_opts.password = password;

    rc = MQTTClient_connect(handle, &conn_opts);
    if (rc != MQTTCLIENT_SUCCESS) {
        LOG_E("Failed to connect, return code %d\n", rc);
        return -EMBER_ERROR;
    }
    return EMBER_EOK;
}

int msg_bus_disconnect(msg_node_t handle)
{
    MQTTClient_disconnect(handle, 10000);
    return EMBER_EOK;
}

int msg_bus_is_connected(msg_node_t handle)
{
    if (MQTTClient_isConnected(handle)) return EMBER_TRUE;
    else return EMBER_FALSE;
}

#ifdef __cplusplus
}
#endif

#endif /* EMBER_LIBS_USING_MQTT_ASYNC */