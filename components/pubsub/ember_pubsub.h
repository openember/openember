/*
 * Copyright (c) 2025, OpenEmber Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * OpenEmber pub/sub 通信骨架（UDP / ZeroMQ / NNG / LCM）。
 * 与现有 MQTT msg_bus 并行存在，后续可接入统一 transport 抽象层。
 */

#ifndef OPENEMBER_EMBER_PUBSUB_H
#define OPENEMBER_EMBER_PUBSUB_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ember_pubsub ember_pubsub_t;

/**
 * 收到消息时的回调（在内部接收线程中调用，请避免长时间阻塞）。
 *
 * @param topic        主题字符串（以 '\\0' 结尾的副本）
 * @param payload      负载缓冲区（仅回调期间有效；回调返回后由库释放）
 * @param payload_len  负载长度
 * @param user_data    ember_pubsub_create_subscriber 传入的用户指针
 */
typedef void (*ember_pubsub_on_message_t)(const char *topic, const void *payload,
                                          size_t payload_len, void *user_data);

/**
 * 创建发布节点。
 *
 * @param bind_url
 *               - UDP:  "udp://127.0.0.1:7560"（发布端 sendto 目标）
 *               - ZMQ:  "tcp://*:5560"（由 libzmq 的 zmq_bind 处理）
 *               - NNG:  "tcp://127.0.0.1:5560"（由 nng_listen 处理）
 *               - LCM:  lcm_create() provider 字符串，例如 "udpm://239.255.76.67:7667"
 * @return 0 成功，非 0 失败
 */
int ember_pubsub_create_publisher(ember_pubsub_t **out, const char *bind_url);

/**
 * 创建订阅节点并启动接收线程。
 *
 * @param connect_url
 *                    - UDP: "udp://127.0.0.1:7560"（订阅端绑定该端口收包）
 *                    - ZMQ: "tcp://127.0.0.1:5560"
 *                    - NNG: "tcp://127.0.0.1:5560"
 *                    - LCM: lcm_create() provider 字符串，例如 "udpm://239.255.76.67:7667"
 * @param on_message  可为 NULL（仅用于测试占位，不会收到通知）
 * @param user_data   传给回调
 */
int ember_pubsub_create_subscriber(ember_pubsub_t **out, const char *connect_url,
                                   ember_pubsub_on_message_t on_message, void *user_data);

/**
 * 订阅主题前缀（ZeroMQ 前缀匹配）。可多次调用。
 * 若从未调用，库会默认订阅空前缀（接收全部主题）。
 */
int ember_pubsub_subscribe(ember_pubsub_t *ps, const char *topic_prefix);

/**
 * 发布一条消息：两帧 [topic][payload]，与订阅端一致。
 *
 * @param topic 必须以 '\\0' 结尾的 C 字符串；发送长度为 strlen(topic)
 */
int ember_pubsub_publish(ember_pubsub_t *ps, const char *topic, const void *payload,
                         size_t payload_len);

/** 销毁节点；订阅端会停止接收线程并 join。 */
void ember_pubsub_destroy(ember_pubsub_t *ps);

#ifdef __cplusplus
}
#endif

#endif /* OPENEMBER_EMBER_PUBSUB_H */
