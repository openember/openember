/*
 * Copyright (c) 2022-2023, OpenEmber Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2022-07-07     luhuadong    the first version
 */

#ifndef __AG_ZMQ_WRAPPER_H__
#define __AG_ZMQ_WRAPPER_H__

#include "agloo.h"

#ifdef AG_LIBS_USING_ZEROMQ

#ifdef __cplusplus
extern "C" {
#endif

typedef void* msg_node_t;

/* Callback function pointer type */
typedef void msg_arrived_cb_t(char *topic, char *payload);

int msg_bus_init(msg_node_t *handle, const char *name, char *address, msg_arrived_cb_t *cb);
int msg_bus_deinit(msg_node_t handle);

int msg_bus_publish(msg_node_t handle, const char *topic, const char *payload);
int msg_bus_subscribe(msg_node_t handle, const char *topic);

#ifdef __cplusplus
}
#endif

#endif /* AG_LIBS_USING_ZEROMQ */

#endif /* __AG_ZMQ_WRAPPER_H__ */
