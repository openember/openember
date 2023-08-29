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
#if defined (AG_LIBS_USING_MQTT) && defined (AG_LIBS_USING_MQTT_ASYNC) && ! defined(AG_LIBS_USING_MQTT_CLIENT)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "MQTTAsync.h"
#include "MQTTClient.h"
#include "message.h"

#define ADDRESS     "tcp://localhost:1883"
#define QOS         1
#define TIMEOUT     10000L

#ifdef __cplusplus
extern "C" {
#endif

static const char *username = "openember";
static const char *password = "p@ssw0rd";

#ifdef AG_LIBS_USING_MQTT_ASYNC
static void delivered(void *context, MQTTAsync_token token)
{
    //printf("\nMessage with token value %d delivery confirmed\n", token);
}

static int msgarrvd(void *context, char *topicName, int topicLen, MQTTAsync_message *message)
{
    msg_arrived_cb_t *user_cb = (msg_arrived_cb_t *)context;

    //printf("\nMessage arrived\n");
    //printf("     topic: %s\n", topicName);
    //printf("   message: %.*s\n", message->payloadlen, (char*)message->payload);

    user_cb(topicName, message->payload, message->payloadlen);

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
    msg_arrived_cb_t *user_cb = (msg_arrived_cb_t *)context;

    //printf("\nMessage arrived\n");
    //printf("     topic: %s\n", topicName);
    //printf("   message: %.*s\n", message->payloadlen, (char*)message->payload);

    user_cb(topicName, message->payload, message->payloadlen);

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

int msg_bus_init(msg_node_t *handle, const char *name, char *address, msg_arrived_cb_t *cb)
{
    int rc;

#ifdef AG_LIBS_USING_MQTT_ASYNC
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
        return -AG_ERROR;
    }

    rc = MQTTAsync_setCallbacks(*handle, (void *)cb, connlost, msgarrvd, delivered);
    if (rc != MQTTASYNC_SUCCESS) {
        LOG_E("Failed to set callbacks, return code %d\n", rc);
        MQTTAsync_destroy(handle);
        return -AG_ERROR;
    }

    rc = MQTTAsync_connect(*handle, &conn_opts);
    if (rc != MQTTASYNC_SUCCESS) {
        LOG_E("Failed to connect, return code %d\n", rc);
        LOG_E("Reason: %s\n", MQTTReasonCode_toString(rc));
        MQTTAsync_destroy(handle);
        return -AG_ERROR;
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
        return -AG_ERROR;
    }

    rc = MQTTClient_setCallbacks(*handle, (void *)cb, connlost, msgarrvd, delivered);
    if (rc != MQTTCLIENT_SUCCESS) {
        LOG_E("Failed to set callbacks, return code %d\n", rc);
        MQTTClient_destroy(handle);
        return -AG_ERROR;
    }

    //使用MQTTClient_connect将client连接到服务器，使用指定的连接选项。成功则返回MQTTCLIENT_SUCCESS
    rc = MQTTClient_connect(*handle, &conn_opts);
    if (rc != MQTTCLIENT_SUCCESS) {
        LOG_E("Failed to connect, return code %d\n", rc);
        MQTTClient_destroy(handle);
        return -AG_ERROR;
    }
#endif

    return AG_EOK;
}

int msg_bus_deinit(msg_node_t handle)
{
    MQTTClient_destroy(&handle);
    return AG_EOK;
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
        return -AG_ERROR;
    }

    return AG_EOK;
}

int msg_bus_publish_raw(msg_node_t handle, const char *topic, const void *payload, const int payloadlen)
{
    int rc;
    MQTTClient_deliveryToken token;

    MQTTClient_publish(handle, topic, payloadlen, payload, QOS, 0, &token);
    rc = MQTTClient_waitForCompletion(handle, token, TIMEOUT);
    if (rc != MQTTCLIENT_SUCCESS) {
        LOG_E("Failed to publish, return code %d\n", rc);
        return -AG_ERROR;
    }

    return AG_EOK;
}

int msg_bus_subscribe(msg_node_t handle, const char *topic)
{
    int rc;

#ifdef AG_LIBS_USING_MQTT_ASYNC
    MQTTAsync_responseOptions response = MQTTAsync_responseOptions_initializer;
    rc = MQTTAsync_subscribe(handle, topic, QOS, &response);
    if (rc != MQTTASYNC_SUCCESS) {
        LOG_E("Failed to subscribe, return code %d\n", rc);
        return -AG_ERROR;
    }
#else
    rc = MQTTClient_subscribe(handle, topic, QOS);
    if (rc != MQTTCLIENT_SUCCESS) {
        LOG_E("Failed to subscribe, return code %d\n", rc);
        return -AG_ERROR;
    }
#endif

    return AG_EOK;
}

int msg_bus_unsubscribe(msg_node_t handle, const char *topic)
{
    int rc;
    rc = MQTTClient_unsubscribe(handle, topic);
    if (rc != MQTTCLIENT_SUCCESS) {
        LOG_E("Failed to unsubscribe, return code %d\n", rc);
        return -AG_ERROR;
    }
    return AG_EOK;
}

int msg_bus_set_callback(msg_node_t handle, msg_arrived_cb_t *cb)
{
    int rc;

    rc = MQTTClient_setCallbacks(handle, (void *)cb, connlost, msgarrvd, delivered);
    if (rc != MQTTCLIENT_SUCCESS) {
        LOG_E("Failed to set callbacks, return code %d\n", rc);
        return -AG_ERROR;
    }
    return AG_EOK;
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
        return -AG_ERROR;
    }
    return AG_EOK;
}

int msg_bus_disconnect(msg_node_t handle)
{
    MQTTClient_disconnect(handle, 10000);
    return AG_EOK;
}

int msg_bus_is_connected(msg_node_t handle)
{
    if (MQTTClient_isConnected(handle)) return AG_TRUE;
    else return AG_FALSE;
}

#ifdef __cplusplus
}
#endif

#endif /* AG_LIBS_USING_MQTT_ASYNC */