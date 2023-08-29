/*
 * Copyright (c) 2022-2023, OpenEmber Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2022-07-07     luhuadong    the first version
 * 2022-11-08     luhuadong    add msg_bus_recv & msg_bus_free
 */

#ifndef __AG_MQTT_WRAPPER_H__
#define __AG_MQTT_WRAPPER_H__

#include "openember.h"

#ifdef AG_LIBS_USING_MQTT

#ifdef __cplusplus
extern "C" {
#endif

/*
 * MQTTClient, MQTTAsync ...
 */
typedef void* msg_node_t;

/* Callback function pointer type */
typedef void msg_arrived_cb_t(char *topic, void *payload, size_t payloadlen);
typedef void msg_connlost_cb_t(char *cause);
typedef void msg_delivered_cb_t(int token);

int msg_bus_init(msg_node_t *handle, const char *name, char *address, msg_arrived_cb_t *cb);
int msg_bus_deinit(msg_node_t handle);

int msg_bus_publish(msg_node_t handle, const char *topic, const char *payload);
int msg_bus_publish_raw(msg_node_t handle, const char *topic, const void *payload, const int payloadlen);

int msg_bus_subscribe(msg_node_t handle, const char *topic);
int msg_bus_unsubscribe(msg_node_t handle, const char *topic);

int msg_bus_set_callback(msg_node_t handle, msg_arrived_cb_t *cb);

int msg_bus_connect(msg_node_t handle);
int msg_bus_disconnect(msg_node_t handle);
int msg_bus_is_connected(msg_node_t handle);

#ifdef AG_LIBS_USING_MQTT_ASYNC
int msg_bus_send(msg_node_t handle, const char *topic, const char *payload);
#endif
int msg_bus_recv(msg_node_t handle, char** topicName, void** payload, int* payloadLen, time_t timeout);

void msg_bus_free(void *topic, void *payload);

#ifdef __cplusplus
}
#endif

#endif /* AG_LIBS_USING_MQTT */

#endif /* __AG_MQTT_WRAPPER_H__ */
