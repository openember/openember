#include "openember/transport/service_server.hpp"
#include "openember/transport/session.hpp"

#include <chrono>
#include <memory>
#include <string>

#include "zenoh.hxx"

namespace openember::transport {

class ServiceServer::Impl {
public:
    explicit Impl(zenoh::Queryable<void> queryable)
        : queryable_(std::move(queryable)) {}

private:
    zenoh::Queryable<void> queryable_;
};

ServiceServer::ServiceServer() = default;
ServiceServer::~ServiceServer() = default;
ServiceServer::ServiceServer(ServiceServer&&) noexcept = default;
ServiceServer& ServiceServer::operator=(ServiceServer&&) noexcept = default;

ServiceServer::ServiceServer(std::unique_ptr<Impl> impl)
    : impl_(std::move(impl)) {}

Result<ServiceServer> Session::CreateServiceServer(
    const ServiceOptions& options,
    ServiceCallback callback) {
    if (!IsOpen()) {
        return Error{ErrorCode::kNotInitialized, "session is not open"};
    }

    try {
        const auto key = Keys().ServiceKey(options.name);

        auto queryable = transport_internal::RawZenohSession(*this).declare_queryable(
            zenoh::KeyExpr(key),
            [key, callback = std::move(callback)](const zenoh::Query& query) {
                Message request;
                request.key = std::string(query.get_keyexpr().as_string_view());
                request.receive_time = std::chrono::steady_clock::now();

                auto response = callback(request);
                if (!response.Ok()) {
                    query.reply(
                        zenoh::KeyExpr(key),
                        zenoh::Bytes(response.Err().message)
                    );
                    return;
                }

                query.reply(
                    zenoh::KeyExpr(key),
                    zenoh::Bytes(response.Value())
                );
            },
            zenoh::closures::none
        );

        return ServiceServer(std::make_unique<ServiceServer::Impl>(std::move(queryable)));
    } catch (const std::exception& e) {
        return Error{
            ErrorCode::kZenohError,
            std::string("failed to create service server: ") + e.what()
        };
    }
}

}  // namespace openember::transport
