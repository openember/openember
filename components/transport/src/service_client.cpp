#include "openember/transport/service_client.hpp"
#include "openember/transport/session.hpp"

#include <chrono>
#include <atomic>
#include <future>
#include <memory>
#include <string>

#include "zenoh.hxx"

namespace openember::transport {

class ServiceClient::Impl {
public:
    Impl(zenoh::Session* session, std::string key, std::size_t maximum)
        : session_(session),
          key_(std::move(key)), maximum_(maximum) {}

    Result<ByteBuffer> Call(const ByteBuffer& request,
                            std::chrono::milliseconds timeout) {
        if (session_ == nullptr) {
            return Error{ErrorCode::kNotInitialized, "session is null"};
        }

        try {
            // A query callback may outlive Call() on timeout, and more than one
            // queryable may reply. Own the state until Zenoh releases it and
            // complete the promise at most once.
            struct PendingCall {
                std::promise<Result<ByteBuffer>> promise;
                std::atomic<bool> completed{false};
            };
            auto pending = std::make_shared<PendingCall>();
            auto future = pending->promise.get_future();

            auto on_reply = [pending, maximum = maximum_](const zenoh::Reply& reply) {
                if (pending->completed.exchange(true)) return;
                if (!reply.is_ok()) {
                    pending->promise.set_value(Error{
                        ErrorCode::kZenohError,
                        "zenoh reply error"
                    });
                    return;
                }

                const auto& sample = reply.get_ok();
                const auto& payload = sample.get_payload();
                if (maximum && payload.size() > maximum) {
                    pending->promise.set_value(Error{ErrorCode::kInvalidArgument, "service reply exceeds payload limit"});
                    return;
                }
                ByteBuffer buffer;
                // Empty replies are valid (e.g. an empty protobuf message).
                if (payload.size() != 0) buffer = payload.as_vector();
                pending->promise.set_value(std::move(buffer));
            };

            auto on_done = []() {};

            zenoh::Session::GetOptions opts;
            opts.payload = zenoh::Bytes(request);
            if (timeout.count() > 0) {
                opts.timeout_ms = static_cast<uint64_t>(timeout.count());
            }

            session_->get(
                zenoh::KeyExpr(key_),
                "",
                on_reply,
                on_done,
                std::move(opts)
            );

            if (future.wait_for(timeout) != std::future_status::ready) {
                pending->completed.store(true);
                return Error{ErrorCode::kTimeout, "service call timeout"};
            }

            return future.get();
        } catch (const std::exception& e) {
            return Error{
                ErrorCode::kZenohError,
                std::string("service call failed: ") + e.what()
            };
        }
    }

private:
    zenoh::Session* session_ = nullptr;
    std::string key_;
    std::size_t maximum_ = 0;
};

ServiceClient::ServiceClient() = default;
ServiceClient::~ServiceClient() = default;
ServiceClient::ServiceClient(ServiceClient&&) noexcept = default;
ServiceClient& ServiceClient::operator=(ServiceClient&&) noexcept = default;

ServiceClient::ServiceClient(std::unique_ptr<Impl> impl)
    : impl_(std::move(impl)) {}

Result<ByteBuffer> ServiceClient::Call(const ByteBuffer& request,
                                       std::chrono::milliseconds timeout) {
    if (!impl_) {
        return Error{ErrorCode::kNotInitialized, "service client not initialized"};
    }

    return impl_->Call(request, timeout);
}

Result<ServiceClient> Session::CreateServiceClient(const ServiceOptions& options) {
    if (!IsOpen()) {
        return Error{ErrorCode::kNotInitialized, "session is not open"};
    }

    try {
        const auto key = Keys().ServiceKey(options.name);

        return ServiceClient(std::make_unique<ServiceClient::Impl>(
            &transport_internal::RawZenohSession(*this),
            key, options.max_payload_bytes
        ));
    } catch (const std::exception& e) {
        return Error{
            ErrorCode::kZenohError,
            std::string("failed to create service client: ") + e.what()
        };
    }
}

}  // namespace openember::transport
