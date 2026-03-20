/*
 * Copyright (c) 2022-2023, OpenEmber Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2022-07-07     luhuadong    the first version
 */

#ifndef __EMBER_MSG_WRAPPER_H__
#define __EMBER_MSG_WRAPPER_H__

#include "openember.h"

#ifdef EMBER_LIBS_USING_MQTT
#include "mqtt_wrapper.h"
#endif

#ifdef EMBER_LIBS_USING_ZEROMQ
#include "zmq_wrapper.h"
#endif

#ifdef EMBER_LIBS_USING_NNG
#include "nng_wrapper.h"
#endif

#ifdef EMBER_LIBS_USING_LCM
#include "lcm_wrapper.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

int msg_keepalive_update(msg_node_t handle, const char *name, mod_class_t cls, state_t state);
int msg_smm_register(msg_node_t handle, const char *name, mod_class_t cls);

#ifdef __cplusplus
}
#endif

#endif /* __EMBER_MSG_WRAPPER_H__ */