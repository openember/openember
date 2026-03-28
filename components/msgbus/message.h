/*
 * Copyright (c) 2022-2023, OpenEmber Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * 框架内部消息总线（msg_bus_* 声明由各 backend 提供）：节点间模块通信、SMM、订阅框架话题。
 * 应用若只需「任意 topic 数据流」（如 spdlog 对外广播），请用 components/pubsub/ember_pubsub.h，
 * 勿与 msg_bus 混为一谈。
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