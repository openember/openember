#include "openember/transport/session.hpp"

#include <memory>
#include <mutex>
#include <string>

#include "zenoh.hxx"

namespace openember::transport {

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

            // 第一版可以先不处理 mode/connect/listen。
            // 等基本 pub/sub 跑通后，再补：
            // config.insert_json5("mode", "\"peer\"");
            // config.insert_json5("connect/endpoints", "[\"tcp/127.0.0.1:7447\"]");

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
    friend class Session;
    friend class Publisher;
    friend class Subscriber;
    friend class ServiceServer;
    friend class ServiceClient;

    SessionOptions options_;
    KeyBuilder key_builder_;
    std::unique_ptr<zenoh::Session> session_;
    bool opened_ = false;
    mutable std::mutex mutex_;
};

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