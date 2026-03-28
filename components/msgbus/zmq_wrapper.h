/*
 * Copyright (c) 2022-2023, OpenEmber Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2022-07-07     luhuadong    the first version
 */

#ifndef __EMBER_ZMQ_WRAPPER_H__
#define __EMBER_ZMQ_WRAPPER_H__

#include "openember.h"

#ifdef EMBER_LIBS_USING_ZEROMQ

#ifdef __cplusplus
extern "C" {
#endif

typedef void* msg_node_t;

/* Callback function pointer type */
typedef void msg_arrived_cb_t(void *user_data, char *topic, void *payload, size_t payloadlen);

int msg_bus_init(msg_node_t *handle, const char *name, char *address, msg_arrived_cb_t *cb,
                 void *user_data);
int msg_bus_deinit(msg_node_t handle);

int msg_bus_publish(msg_node_t handle, const char *topic, const char *payload);
int msg_bus_subscribe(msg_node_t handle, const char *topic);

#ifdef __cplusplus
}
#endif

#endif /* EMBER_LIBS_USING_ZEROMQ */

#endif /* __EMBER_ZMQ_WRAPPER_H__ */
