#pragma once

#include <chrono>
#include <memory>
#include <optional>
#include <string>

#include "openember/serialization.hpp"
#include "openember/transport/service_client.hpp"

namespace openember {

class ClientBase {
public:
    ClientBase(std::string service_name,
               std::string request_type,
               std::string response_type,
               transport::ServiceClient raw_client);

    transport::Result<transport::ByteBuffer> CallRaw(
        const transport::ByteBuffer& request,
        std::chrono::milliseconds timeout);

    const std::string& ServiceName() const;
    const std::string& RequestType() const;
    const std::string& ResponseType() const;

private:
    std::string service_name_;
    std::string request_type_;
    std::string response_type_;
    transport::ServiceClient raw_client_;
};

template <typename RequestT, typename ResponseT>
class Client {
public:
    Client() = default;

    explicit Client(std::shared_ptr<ClientBase> base)
        : base_(std::move(base)) {}

    std::optional<ResponseT> Call(const RequestT& request,
                                  std::chrono::milliseconds timeout) {
        if (!base_) {
            return std::nullopt;
        }

        auto request_payload = Serialize<RequestT>(request);
        auto result = base_->CallRaw(request_payload, timeout);

        if (!result.Ok()) {
            return std::nullopt;
        }

        return Deserialize<ResponseT>(result.Value());
    }

    bool Valid() const {
        return static_cast<bool>(base_);
    }

private:
    std::shared_ptr<ClientBase> base_;
};

}  // namespace openember