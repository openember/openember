/*
 * C++ TransportBackend for LCM using lcm::LCM (header wrapper over lcm.h).
 *
 * Wire format matches lcm_wrapper.c: single channel + [topic][0x00][payload].
 */
#include "transport_backend.hpp"

#include "openember.h"

#include <lcm/lcm-cpp.hpp>

#include <atomic>
#include <cerrno>
#include <cstring>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace openember::msgbus {

namespace {

constexpr char kChannel[] = "openember/msgbus";
constexpr char kDefaultProvider[] = "udpm://239.255.76.67:7667";
constexpr std::size_t kMaxSubs = 64;

static bool split_topic_payload(const void *buf, std::size_t len, std::string *out_topic,
                                std::vector<unsigned char> *out_payload)
{
    if (!buf || !out_topic || !out_payload) {
        return false;
    }
    const auto *p = static_cast<const unsigned char *>(buf);
    for (std::size_t i = 0; i < len; i++) {
        if (p[i] == 0x00u) {
            out_topic->assign(static_cast<const char *>(buf), i);
            const std::size_t payload_len = (len > (i + 1u)) ? (len - (i + 1u)) : 0u;
            out_payload->resize(payload_len);
            if (payload_len > 0u) {
                std::memcpy(out_payload->data(), p + (i + 1u), payload_len);
            }
            return true;
        }
    }
    return false;
}

static bool topic_matches(const std::vector<std::string> &subs, const char *topic)
{
    if (!topic || subs.empty()) {
        return false;
    }
    for (const auto &sub : subs) {
        if (sub.empty()) {
            continue;
        }
        if (std::strncmp(topic, sub.c_str(), sub.size()) == 0) {
            return true;
        }
    }
    return false;
}

} // namespace

class LcmTransportBackend final : public TransportBackend {
public:
    int init(const std::string &node_name, const std::string &address, MessageCallback cb) override
    {
        (void)node_name;
        (void)deinit();

        const std::string url = address.empty() ? std::string(kDefaultProvider) : address;
        lcm_ = std::make_unique<lcm::LCM>(url);
        if (!lcm_->good()) {
            lcm_.reset();
            return -EMBER_ERROR;
        }

        cb_ = std::move(cb);

        sub_ = lcm_->subscribe(std::string(kChannel), &LcmTransportBackend::onLcmRawMethod, this);
        if (!sub_) {
            lcm_.reset();
            return -EMBER_ERROR;
        }

        if (cb_) {
            recv_running_.store(true);
            recv_thread_ = std::thread([this] { recvLoop(); });
        }

        return EMBER_EOK;
    }

    int deinit() override
    {
        recv_running_.store(false);
        if (recv_thread_.joinable()) {
            recv_thread_.join();
        }
        sub_ = nullptr;
        lcm_.reset();
        cb_ = {};
        {
            std::lock_guard<std::mutex> lock(mtx_);
            subs_.clear();
        }
        return EMBER_EOK;
    }

    int publish(std::string_view topic, std::string_view payload_text) override
    {
        return publish_raw(topic, payload_text.data(), payload_text.size());
    }

    int publish_raw(std::string_view topic, const void *payload, std::size_t payload_len) override
    {
        if (!lcm_ || topic.empty()) {
            return -EINVAL;
        }
        if (payload_len > 0 && !payload) {
            return -EINVAL;
        }

        const std::size_t tlen = topic.size();
        const std::size_t plen = payload_len;
        std::vector<unsigned char> msg(tlen + 1u + plen);
        if (tlen > 0u) {
            std::memcpy(msg.data(), topic.data(), tlen);
        }
        msg[tlen] = 0x00u;
        if (plen > 0u) {
            std::memcpy(msg.data() + (tlen + 1u), payload, plen);
        }

        const int rv = lcm_->publish(std::string(kChannel), msg.data(), static_cast<unsigned int>(msg.size()));
        return (rv == 0) ? EMBER_EOK : -EMBER_ERROR;
    }

    int subscribe(std::string_view topic_prefix) override
    {
        std::string t(topic_prefix);
        std::lock_guard<std::mutex> lock(mtx_);
        for (const auto &s : subs_) {
            if (s == t) {
                return EMBER_EOK;
            }
        }
        if (subs_.size() >= kMaxSubs) {
            return -ENOMEM;
        }
        subs_.push_back(std::move(t));
        return EMBER_EOK;
    }

    int unsubscribe(std::string_view topic_prefix) override
    {
        std::string t(topic_prefix);
        std::lock_guard<std::mutex> lock(mtx_);
        for (auto it = subs_.begin(); it != subs_.end(); ++it) {
            if (*it == t) {
                subs_.erase(it);
                return EMBER_EOK;
            }
        }
        return EMBER_EOK;
    }

    ~LcmTransportBackend() override
    {
        (void)deinit();
    }

private:
    void onLcmRawMethod(const lcm::ReceiveBuffer *rbuf, const std::string &channel)
    {
        (void)channel;
        dispatchIncoming(rbuf);
    }

    void dispatchIncoming(const lcm::ReceiveBuffer *rbuf)
    {
        if (!rbuf || !cb_) {
            return;
        }

        std::string topic;
        std::vector<unsigned char> payload;
        if (!split_topic_payload(rbuf->data, rbuf->data_size, &topic, &payload)) {
            return;
        }

        bool matched = false;
        {
            std::lock_guard<std::mutex> lock(mtx_);
            matched = topic_matches(subs_, topic.c_str());
        }
        if (!matched) {
            return;
        }

        cb_(std::string_view(topic), payload.empty() ? nullptr : payload.data(), payload.size());
    }

    void recvLoop()
    {
        while (recv_running_.load()) {
            (void)lcm_->handleTimeout(200);
        }
    }

    std::unique_ptr<lcm::LCM> lcm_;
    lcm::Subscription *sub_{nullptr};
    std::thread recv_thread_;
    std::atomic<bool> recv_running_{false};

    std::mutex mtx_;
    std::vector<std::string> subs_;

    MessageCallback cb_{};
};

std::unique_ptr<TransportBackend> detail_create_lcm_cpp_transport_backend()
{
    return std::make_unique<LcmTransportBackend>();
}

} // namespace openember::msgbus
