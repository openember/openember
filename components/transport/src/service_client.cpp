#include "openember/transport/service_client.hpp"
#include "openember/transport/session.hpp"

#include <chrono>
#include <future>
#include <memory>
#include <string>

#include "zenoh.hxx"

namespace openember::transport {

class ServiceClient::Impl {
public:
    Impl(zenoh::Session* session, std::string key)
        : session_(session),
          key_(std::move(key)) {}

    Result<ByteBuffer> Call(const ByteBuffer& request,
                            std::chrono::milliseconds timeout) {
        if (session_ == nullptr) {
            return Error{ErrorCode::kNotInitialized, "session is null"};
        }

        try {
            std::promise<Result<ByteBuffer>> promise;
            auto future = promise.get_future();

            auto on_reply = [&promise](const zenoh::Reply& reply) mutable {
                if (!reply.is_ok()) {
                    promise.set_value(Error{
                        ErrorCode::kZenohError,
                        "zenoh reply error"
                    });
                    return;
                }

                const auto& sample = reply.get_ok();
                const auto payload = sample.get_payload();
                const auto bytes = payload.as_vector();

                ByteBuffer buffer;
                buffer.assign(bytes.begin(), bytes.end());

                promise.set_value(std::move(buffer));
            };

            auto on_done = []() {};

            // 不同 zenoh-cpp 版本对 query payload 的支持方式可能不同。
            // 第一版可以先将 request 编码到 parameters。
            session_->get(
                zenoh::KeyExpr(key_),
                "",
                on_reply,
                on_done
            );

            if (future.wait_for(timeout) != std::future_status::ready) {
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
    if (!impl_->IsOpen()) {
        return Error{ErrorCode::kNotInitialized, "session is not open"};
    }

    try {
        const auto key = impl_->Keys().ServiceKey(options.name);

        return ServiceClient(std::make_unique<ServiceClient::Impl>(
            &impl_->RawSession(),
            key
        ));
    } catch (const std::exception& e) {
        return Error{
            ErrorCode::kZenohError,
            std::string("failed to create service client: ") + e.what()
        };
    }
}

}  // namespace openember::transport