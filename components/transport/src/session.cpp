#include "openember/transport/session.hpp"

#include <memory>
#include <mutex>
#include <string>

#include "zenoh.hxx"

namespace openember::transport {
namespace {

std::string ZenohJsonString(const std::string& value) {
    return "\"" + value + "\"";
}

std::string ZenohEndpointArrayJson(const std::string& endpoint) {
    return "[\"" + endpoint + "\"]";
}

const char* ZenohModeString(ZenohMode mode) {
    switch (mode) {
        case ZenohMode::kClient:
            return "client";
        case ZenohMode::kRouter:
            return "router";
        case ZenohMode::kPeer:
        default:
            return "peer";
    }
}

void ApplySessionOptions(zenoh::Config& config, const SessionOptions& options) {
    config.insert_json5("mode", ZenohJsonString(ZenohModeString(options.mode)));

    if (!options.listen.empty()) {
        config.insert_json5("listen/endpoints", ZenohEndpointArrayJson(options.listen));
    }
    if (!options.connect.empty()) {
        config.insert_json5("connect/endpoints", ZenohEndpointArrayJson(options.connect));
    }

    // 显式 TCP 时关闭组播 scouting，与 zenoh-cpp 连通性测试一致，避免本机双进程发现失败。
    if (!options.listen.empty() || !options.connect.empty()) {
        config.insert_json5("scouting/multicast/enabled", "false");
        config.insert_json5("scouting/gossip/enabled", "false");
    }
}

}  // namespace

class Session::Impl {
public:
    explicit Impl(SessionOptions options)
        : options_(std::move(options)),
          key_builder_(options_.robot_id, options_.namespace_name) {}

    Result<void> Open() {
        std::lock_guard<std::mutex> lock(mutex_);

        if (opened_) {
            return {};
        }

        try {
            zenoh::Config config = zenoh::Config::create_default();
            ApplySessionOptions(config, options_);

            session_ = std::make_unique<zenoh::Session>(
                zenoh::Session::open(std::move(config))
            );

            opened_ = true;
            return {};
        } catch (const std::exception& e) {
            return Error{
                ErrorCode::kZenohError,
                std::string("failed to open zenoh session: ") + e.what()
            };
        }
    }

    void Close() {
        std::lock_guard<std::mutex> lock(mutex_);
        session_.reset();
        opened_ = false;
    }

    bool IsOpen() const {
        return opened_;
    }

    zenoh::Session& RawSession() {
        return *session_;
    }

    const KeyBuilder& Keys() const {
        return key_builder_;
    }

private:
    SessionOptions options_;
    KeyBuilder key_builder_;
    std::unique_ptr<zenoh::Session> session_;
    bool opened_ = false;
    mutable std::mutex mutex_;
};

namespace transport_internal {

zenoh::Session& RawZenohSession(Session& session) {
    return session.impl_->RawSession();
}

}  // namespace transport_internal

Session::Session(const SessionOptions& options)
    : impl_(std::make_unique<Impl>(options)) {}

Session::~Session() = default;

Session::Session(Session&&) noexcept = default;
Session& Session::operator=(Session&&) noexcept = default;

Result<void> Session::Open() {
    return impl_->Open();
}

void Session::Close() {
    impl_->Close();
}

bool Session::IsOpen() const {
    return impl_->IsOpen();
}

const KeyBuilder& Session::Keys() const {
    return impl_->Keys();
}

}  // namespace openember::transport
