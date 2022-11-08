/*
 * Copyright (c) 2006-2020, DreamGrow Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2022-07-07     luhuadong    the first version
 */

#define LOG_TAG "MSG"
#include "agloo.h"
#ifdef AG_LIBS_USING_ZEROMQ

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <zmq.h>

#ifdef __cplusplus
extern "C" {
#endif

int msg_bus_init(msg_node_t *handle, const char *name, char *address, msg_arrived_cb_t *cb)
{
    return AG_EOK;
}

int msg_bus_deinit(msg_node_t handle)
{
    return AG_EOK;
}

int msg_bus_publish(msg_node_t handle, const char *topic, const char *payload)
{
    return AG_EOK;
}

int msg_bus_subscribe(msg_node_t handle, const char *topic)
{
    return AG_EOK;
}

#ifdef __cplusplus
}
#endif

#endif /* AG_LIBS_USING_ZEROMQ */