#pragma once

#include <memory>

#include "openember/transport/key_builder.hpp"
#include "openember/transport/options.hpp"
#include "openember/transport/publisher.hpp"
#include "openember/transport/result.hpp"
#include "openember/transport/service_client.hpp"
#include "openember/transport/service_server.hpp"
#include "openember/transport/subscriber.hpp"

namespace zenoh {
class Session;
}

namespace openember::transport {

class Session;

namespace transport_internal {
zenoh::Session& RawZenohSession(Session& session);
}

class Session {
public:
    explicit Session(const SessionOptions& options);
    ~Session();

    Session(Session&&) noexcept;
    Session& operator=(Session&&) noexcept;

    Session(const Session&) = delete;
    Session& operator=(const Session&) = delete;

    Result<void> Open();
    void Close();

    bool IsOpen() const;

    Result<Publisher> CreatePublisher(const TopicOptions& options);

    Result<Subscriber> CreateSubscriber(const TopicOptions& options,
                                        MessageCallback callback);

    Result<ServiceServer> CreateServiceServer(const ServiceOptions& options,
                                              ServiceCallback callback);

    Result<ServiceClient> CreateServiceClient(const ServiceOptions& options);

    const KeyBuilder& Keys() const;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;

    friend zenoh::Session& transport_internal::RawZenohSession(Session& session);
};

}  // namespace openember::transport
