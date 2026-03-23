/*
 * NNG based internal msgbus wrapper.
 *
 * Message wire format:
 *   [topic bytes][0x00][payload bytes]
 *
 * SUB subscription matches prefix against the raw message buffer,
 * therefore topic bytes must be placed at the beginning of the message.
 */
#ifndef __EMBER_NNG_WRAPPER_H__
#define __EMBER_NNG_WRAPPER_H__

#include "openember.h"

#ifdef EMBER_LIBS_USING_NNG

#ifdef __cplusplus
extern "C" {
#endif

typedef void* msg_node_t;

/* Callback function prototype type */
typedef void msg_arrived_cb_t(char *topic, void *payload, size_t payloadlen);

int msg_bus_init(msg_node_t *handle, const char *name, char *address, msg_arrived_cb_t *cb);
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

#endif /* EMBER_LIBS_USING_NNG */

#endif /* __EMBER_NNG_WRAPPER_H__ */

