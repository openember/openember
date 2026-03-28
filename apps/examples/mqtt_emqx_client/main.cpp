/*
 * 使用 Eclipse Paho MQTT C（经 openember::mqtt::Client）连接 Broker。
 * 默认参数对齐 EMQX 文档示例：https://docs.emqx.com/zh/cloud/latest/connect_to_deployments/c_sdk.html
 *
 * 构建：-DOPENEMBER_EXAMPLE_MQTT_EMQX=ON，或通过 menuconfig 打开对应选项后 genconfig。
 */

#include <chrono>
#include <cstdio>
#include <string>
#include <thread>

#include "mqtt_client.hpp"
#include "openember.h"
#undef LOG_TAG
#define LOG_TAG "mqtt_emqx"
#include "openember/log.h"

#ifndef OPENEMBER_MQTT_EMQX_BROKER_URI
#define OPENEMBER_MQTT_EMQX_BROKER_URI "tcp://broker.emqx.io:1883"
#endif
#ifndef OPENEMBER_MQTT_EMQX_CLIENT_ID
#define OPENEMBER_MQTT_EMQX_CLIENT_ID "openember-mqtt-example"
#endif
#ifndef OPENEMBER_MQTT_EMQX_USERNAME
#define OPENEMBER_MQTT_EMQX_USERNAME ""
#endif
#ifndef OPENEMBER_MQTT_EMQX_PASSWORD
#define OPENEMBER_MQTT_EMQX_PASSWORD ""
#endif
#ifndef OPENEMBER_MQTT_EMQX_TOPIC
#define OPENEMBER_MQTT_EMQX_TOPIC "emqx/c-test"
#endif

int main()
{
    oe_log_init(LOG_TAG);

    openember::mqtt::ClientConfig cfg;
    cfg.server_uri = OPENEMBER_MQTT_EMQX_BROKER_URI;
    cfg.client_id = OPENEMBER_MQTT_EMQX_CLIENT_ID;
    cfg.username = OPENEMBER_MQTT_EMQX_USERNAME;
    cfg.password = OPENEMBER_MQTT_EMQX_PASSWORD;

    openember::mqtt::Client client(std::move(cfg));

    auto on_msg = [](std::string_view topic, const void *payload, std::size_t len) {
        std::string t(topic);
        LOG_I("recv topic=%s payload=%.*s", t.c_str(), (int)len,
              payload ? static_cast<const char *>(payload) : "");
    };

    int rc = client.connect(std::move(on_msg));
    if (rc != 0) {
        LOG_E("MQTT connect failed rc=%d (check broker URI / network / TLS)", rc);
        return 1;
    }
    LOG_I("connected to %s", OPENEMBER_MQTT_EMQX_BROKER_URI);

    rc = client.subscribe(OPENEMBER_MQTT_EMQX_TOPIC, 0);
    if (rc != 0) {
        LOG_E("subscribe failed rc=%d", rc);
        (void)client.disconnect();
        return 1;
    }
    LOG_I("subscribed to %s", OPENEMBER_MQTT_EMQX_TOPIC);

    for (int i = 0; i < 5; i++) {
        char payload[64];
        std::snprintf(payload, sizeof(payload), "openember-hello-%d", i);
        rc = client.publish(OPENEMBER_MQTT_EMQX_TOPIC, payload);
        if (rc != 0) {
            LOG_E("publish failed rc=%d", rc);
        } else {
            LOG_I("published: %s", payload);
        }
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    std::this_thread::sleep_for(std::chrono::seconds(2));
    (void)client.disconnect();
    LOG_I("disconnected");
    return 0;
}
