/*
 * Copyright (c) 2006-2020, DreamGrow Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2022-07-07     luhuadong    the first version
 */

#ifndef __AG_MSG_WRAPPER_H__
#define __AG_MSG_WRAPPER_H__

#include "agloo.h"

#ifdef AG_LIBS_USING_MQTT
#include "mqtt_wrapper.h"
#endif

#ifdef AG_LIBS_USING_ZEROMQ
#include "zmq_wrapper.h"
#endif

int msg_keepalive_update(msg_node_t handle, const char *name, mod_class_t class, state_t state);
int msg_smm_register(msg_node_t handle, const char *name, mod_class_t class, const int pid);

#endif /* __AG_MSG_WRAPPER_H__ */