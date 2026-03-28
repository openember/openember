/*
 * LCM based internal msgbus wrapper.
 *
 * Message wire format (same as NNG wrapper):
 *   [topic bytes][0x00][payload bytes]
 *
 * SUBCRIPTION matches prefix against the raw message buffer, therefore
 * topic bytes must be placed at the beginning of the message.
 *
 * 保留原因：所有直接调用 msg_bus_* 的 C/C++ 代码（launch_manager、Log、MQTT 等）
 * 仍链接到本实现。openember::msgbus::MsgBusNode 在 LCM 且 OPENEMBER_MSGBUS_LCM_USE_CPP=ON
 * 时可改用 lcm_transport_backend.cpp（lcm::LCM），与本文并存，勿删本文件。
 */
#ifndef __EMBER_LCM_WRAPPER_H__
#define __EMBER_LCM_WRAPPER_H__

#include "openember.h"

#ifdef EMBER_LIBS_USING_LCM

#ifdef __cplusplus
extern "C" {
#endif

typedef void* msg_node_t;

/* Callback: user_data is the opaque passed to msg_bus_init (may be NULL). */
typedef void msg_arrived_cb_t(void *user_data, char *topic, void *payload, size_t payloadlen);

int msg_bus_init(msg_node_t *handle, const char *name, char *address, msg_arrived_cb_t *cb,
                 void *user_data);
int msg_bus_deinit(msg_node_t handle);

int msg_bus_publish(msg_node_t handle, const char *topic, const char *payload);
int msg_bus_publish_raw(msg_node_t handle, const char *topic, const void *payload, const int payloadlen);

int msg_bus_subscribe(msg_node_t handle, const char *topic);
int msg_bus_unsubscribe(msg_node_t handle, const char *topic);

int msg_bus_recv(msg_node_t handle, char **topicName, void **payload, int *payloadLen, time_t timeout);
void msg_bus_free(void *topic, void *payload);

#ifdef __cplusplus
}
#endif

#endif /* EMBER_LIBS_USING_LCM */

#endif /* __EMBER_LCM_WRAPPER_H__ */

