#include "openember/transport/publisher.hpp"
#include "openember/transport/session.hpp"

#include <memory>
#include <string>

#include "zenoh.hxx"

namespace openember::transport {

class Publisher::Impl {
public:
    Impl(zenoh::Publisher publisher, std::string key)
        : publisher_(std::move(publisher)),
          key_(std::move(key)) {}

    Result<void> Publish(const ByteBuffer& payload) {
        try {
            publisher_.put(
                zenoh::Bytes(
                    reinterpret_cast<const uint8_t*>(payload.data()),
                    payload.size()
                )
            );
            return {};
        } catch (const std::exception& e) {
            return Error{
                ErrorCode::kZenohError,
                std::string("zenoh publish failed: ") + e.what()
            };
        }
    }

private:
    zenoh::Publisher publisher_;
    std::string key_;
};

Publisher::Publisher() = default;
Publisher::~Publisher() = default;
Publisher::Publisher(Publisher&&) noexcept = default;
Publisher& Publisher::operator=(Publisher&&) noexcept = default;

Publisher::Publisher(std::unique_ptr<Impl> impl)
    : impl_(std::move(impl)) {}

Result<void> Publisher::Publish(const ByteBuffer& payload) {
    if (!impl_) {
        return Error{ErrorCode::kNotInitialized, "publisher not initialized"};
    }
    return impl_->Publish(payload);
}

Result<Publisher> Session::CreatePublisher(const TopicOptions& options) {
    if (!impl_->IsOpen()) {
        return Error{ErrorCode::kNotInitialized, "session is not open"};
    }

    try {
        const auto key = impl_->Keys().TopicKey(options.name);
        auto publisher = impl_->RawSession().declare_publisher(
            zenoh::KeyExpr(key)
        );

        return Publisher(std::make_unique<Publisher::Impl>(
            std::move(publisher),
            key
        ));
    } catch (const std::exception& e) {
        return Error{
            ErrorCode::kZenohError,
            std::string("failed to create publisher: ") + e.what()
        };
    }
}

}  // namespace openember::transport