/*
 * Paho MQTT C (MQTTClient) implementation for openember::mqtt::Client.
 */
#include "mqtt_client.hpp"

#include <MQTTClient.h>

#include <cerrno>
#include <cstring>
#include <string>

namespace openember::mqtt {

namespace {

void on_delivered(void *context, MQTTClient_deliveryToken token)
{
    (void)context;
    (void)token;
}

void on_connection_lost(void *context, char *cause)
{
    (void)context;
    (void)cause;
}

int on_message_arrived(void *context, char *topicName, int topicLen, MQTTClient_message *message)
{
    auto *cb = static_cast<MessageCallback *>(context);
    if (!cb || !(*cb) || !message) {
        if (message) {
            MQTTClient_freeMessage(&message);
        }
        if (topicName) {
            MQTTClient_free(topicName);
        }
        return 0;
    }

    std::string topic_storage;
    if (topicName) {
        if (topicLen > 0) {
            topic_storage.assign(topicName, static_cast<std::size_t>(topicLen));
        } else {
            topic_storage = topicName;
        }
    }

    (*cb)(std::string_view(topic_storage),
          message->payload,
          static_cast<std::size_t>(message->payloadlen));

    MQTTClient_freeMessage(&message);
    MQTTClient_free(topicName);
    return 1;
}

} // namespace

struct Client::Impl {
    ClientConfig cfg{};
    MQTTClient client{nullptr};
    MessageCallback on{};
    MessageCallback *callback_storage{nullptr};
    bool connected{false};

    explicit Impl(ClientConfig c) : cfg(std::move(c)) {}

    ~Impl()
    {
        (void)shutdown();
    }

    int shutdown()
    {
        if (client) {
            if (connected) {
                (void)MQTTClient_disconnect(client, static_cast<int>(cfg.operation_timeout_ms));
            }
            MQTTClient_destroy(&client);
            client = nullptr;
        }
        connected = false;
        delete callback_storage;
        callback_storage = nullptr;
        on = {};
        return 0;
    }
};

Client::Client(ClientConfig cfg) : impl_(std::make_unique<Impl>(std::move(cfg))) {}

Client::~Client()
{
    (void)disconnect();
}

bool Client::is_connected() const
{
    return impl_ && impl_->connected && impl_->client != nullptr;
}

int Client::connect(MessageCallback on_message)
{
    if (!impl_) {
        return -EINVAL;
    }
    Impl &p = *impl_;
    if (p.client) {
        return -EBUSY;
    }

    p.on = std::move(on_message);
    delete p.callback_storage;
    p.callback_storage = nullptr;
    if (p.on) {
        p.callback_storage = new MessageCallback(p.on);
    }

    const std::string &uri = p.cfg.server_uri;
    const std::string &cid = p.cfg.client_id;

    int rc = MQTTClient_create(&p.client,
                               uri.c_str(),
                               cid.c_str(),
                               MQTTCLIENT_PERSISTENCE_NONE,
                               nullptr);
    if (rc != MQTTCLIENT_SUCCESS) {
        return -rc;
    }

    MQTTClient_connectOptions opts = MQTTClient_connectOptions_initializer;
    opts.keepAliveInterval = p.cfg.keepalive_interval;
    opts.cleansession = 1;
    if (!p.cfg.username.empty()) {
        opts.username = p.cfg.username.c_str();
        opts.password = p.cfg.password.c_str();
    }

    if (p.callback_storage) {
        rc = MQTTClient_setCallbacks(p.client,
                                   p.callback_storage,
                                   on_connection_lost,
                                   on_message_arrived,
                                   on_delivered);
        if (rc != MQTTCLIENT_SUCCESS) {
            MQTTClient_destroy(&p.client);
            p.client = nullptr;
            delete p.callback_storage;
            p.callback_storage = nullptr;
            return -rc;
        }
    }

    rc = MQTTClient_connect(p.client, &opts);
    if (rc != MQTTCLIENT_SUCCESS) {
        MQTTClient_destroy(&p.client);
        p.client = nullptr;
        delete p.callback_storage;
        p.callback_storage = nullptr;
        return -rc;
    }

    p.connected = true;
    return 0;
}

int Client::disconnect()
{
    if (!impl_) {
        return -EINVAL;
    }
    return impl_->shutdown();
}

int Client::publish(std::string_view topic, std::string_view payload)
{
    return publish_raw(topic, payload.data(), payload.size());
}

int Client::publish_raw(std::string_view topic, const void *data, std::size_t len)
{
    if (!impl_ || !impl_->client || !impl_->connected) {
        return -EINVAL;
    }
    if (topic.empty()) {
        return -EINVAL;
    }
    if (len > 0 && !data) {
        return -EINVAL;
    }

    Impl &p = *impl_;
    std::string t(topic);

    MQTTClient_message pub = MQTTClient_message_initializer;
    pub.payload = len > 0 ? const_cast<void *>(data) : nullptr;
    pub.payloadlen = static_cast<int>(len);
    pub.qos = p.cfg.qos_publish;
    pub.retained = 0;

    MQTTClient_deliveryToken token = 0;
    int rc = MQTTClient_publishMessage(p.client, t.c_str(), &pub, &token);
    if (rc != MQTTCLIENT_SUCCESS) {
        return -rc;
    }
    rc = MQTTClient_waitForCompletion(p.client, token, static_cast<int>(p.cfg.operation_timeout_ms));
    if (rc != MQTTCLIENT_SUCCESS) {
        return -rc;
    }
    return 0;
}

int Client::subscribe(std::string_view topic, int qos)
{
    if (!impl_ || !impl_->client || !impl_->connected) {
        return -EINVAL;
    }
    if (topic.empty()) {
        return -EINVAL;
    }
    int q = qos;
    if (q < 0) {
        q = impl_->cfg.qos_subscribe;
    }
    if (q < 0 || q > 2) {
        return -EINVAL;
    }
    std::string t(topic);
    int rc = MQTTClient_subscribe(impl_->client, t.c_str(), q);
    if (rc != MQTTCLIENT_SUCCESS) {
        return -rc;
    }
    return 0;
}

int Client::unsubscribe(std::string_view topic)
{
    if (!impl_ || !impl_->client || !impl_->connected) {
        return -EINVAL;
    }
    if (topic.empty()) {
        return -EINVAL;
    }
    std::string t(topic);
    int rc = MQTTClient_unsubscribe(impl_->client, t.c_str());
    if (rc != MQTTCLIENT_SUCCESS) {
        return -rc;
    }
    return 0;
}

} // namespace openember::mqtt
