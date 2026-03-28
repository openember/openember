/*
 * Paho MQTT C (MQTTClient) implementation for openember::mqtt::Client.
 */
#include "mqtt_client.hpp"

#include <MQTTClient.h>

#include <cctype>
#include <cerrno>
#include <cstring>
#include <string>

namespace openember::mqtt {

namespace {

bool uri_prefix_casefold(const std::string &uri, const char *prefix)
{
    const std::size_t n = std::strlen(prefix);
    if (uri.size() < n) {
        return false;
    }
    for (std::size_t i = 0; i < n; i++) {
        if (std::tolower(static_cast<unsigned char>(uri[i])) !=
            static_cast<unsigned char>(prefix[i])) {
            return false;
        }
    }
    return true;
}

/** Paho：ssl://、mqtts:// 与 wss:// 均需 MQTTClient_SSLOptions（WebSocket over TLS 也是 TLS） */
bool uri_needs_ssl_options(const std::string &uri)
{
    return uri_prefix_casefold(uri, "ssl://") || uri_prefix_casefold(uri, "mqtts://") ||
           uri_prefix_casefold(uri, "wss://");
}

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

    MQTTClient_SSLOptions ssl_opts = MQTTClient_SSLOptions_initializer;
    std::string trust_pem_path = p.cfg.ssl_trust_store;
    if (uri_needs_ssl_options(uri)) {
        if (!trust_pem_path.empty()) {
            ssl_opts.trustStore = trust_pem_path.c_str();
            ssl_opts.enableServerCertAuth = 1;
        } else if (!p.cfg.ssl_verify_peer) {
            ssl_opts.enableServerCertAuth = 0;
        }
        opts.ssl = &ssl_opts;
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

const char *connect_return_hint(int connect_ret)
{
    if (connect_ret == 0) {
        return nullptr;
    }
    if (connect_ret == -EINVAL) {
        return "参数无效（如重复 connect）。";
    }
    if (connect_ret == -EBUSY) {
        return "已在连接中，勿重复 connect。";
    }
    switch (connect_ret) {
    case 1:
        return "MQTTCLIENT_FAILURE：通用失败；检查网络、Broker、用户名密码、TLS 与证书。";
    case 10:
        return "MQTTCLIENT_SSL_NOT_SUPPORTED：Paho 未带 OpenSSL 编译，不能使用 ssl://。请安装 libssl-dev（或 openssl-devel），删除构建树中 _deps/openember_paho_mqtt_c* 后重新 CMake 并全量编译。";
    case 14:
        return "MQTTCLIENT_BAD_PROTOCOL：URI 方案被拒绝。几乎总是 ssl:// 与未启用 OpenSSL 的 Paho 同时出现（CMake 未找到 OpenSSL 时 PAHO_WITH_SSL=OFF）。请安装 libssl-dev，清理 Paho 的 FetchContent 目录后重新配置编译。";
    default:
        return nullptr;
    }
}

} // namespace openember::mqtt
