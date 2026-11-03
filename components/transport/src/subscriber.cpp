#include "openember/transport/subscriber.hpp"
#include "openember/transport/session.hpp"

#include <chrono>
#include <memory>
#include <string>

#include "zenoh.hxx"

namespace openember::transport {

class Subscriber::Impl {
public:
    explicit Impl(zenoh::Subscriber<void> subscriber)
        : subscriber_(std::move(subscriber)) {}

private:
    zenoh::Subscriber<void> subscriber_;
};

Subscriber::Subscriber() = default;
Subscriber::~Subscriber() = default;
Subscriber::Subscriber(Subscriber&&) noexcept = default;
Subscriber& Subscriber::operator=(Subscriber&&) noexcept = default;

Subscriber::Subscriber(std::unique_ptr<Impl> impl)
    : impl_(std::move(impl)) {}

Result<Subscriber> Session::CreateSubscriber(const TopicOptions& options,
                                             MessageCallback callback) {
    if (!IsOpen()) {
        return Error{ErrorCode::kNotInitialized, "session is not open"};
    }

    try {
        const auto key = Keys().TopicKey(options.name);

        auto subscriber = transport_internal::RawZenohSession(*this).declare_subscriber(
            zenoh::KeyExpr(key),
            [callback = std::move(callback)](const zenoh::Sample& sample) {
                Message msg;
                msg.key = std::string(sample.get_keyexpr().as_string_view());

                const auto& payload = sample.get_payload();
                // zenoh-cpp 1.10 calls a Rust reader with vector::data() even
                // for zero bytes; that pointer may be null. Empty is legal.
                if (payload.size() != 0) msg.payload = payload.as_vector();
                msg.receive_time = std::chrono::steady_clock::now();

                callback(msg);
            },
            zenoh::closures::none
        );

        return Subscriber(std::make_unique<Subscriber::Impl>(std::move(subscriber)));
    } catch (const std::exception& e) {
        return Error{
            ErrorCode::kZenohError,
            std::string("failed to create subscriber: ") + e.what()
        };
    }
}

}  // namespace openember::transport
