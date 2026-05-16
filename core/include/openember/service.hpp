#pragma once

#include <memory>
#include <string>

#include "openember/transport/service_server.hpp"

namespace openember {

class ServiceBase {
public:
    ServiceBase(std::string service_name,
                std::string request_type,
                std::string response_type,
                transport::ServiceServer raw_server);

    const std::string& ServiceName() const;
    const std::string& RequestType() const;
    const std::string& ResponseType() const;

private:
    std::string service_name_;
    std::string request_type_;
    std::string response_type_;
    transport::ServiceServer raw_server_;
};

template <typename RequestT, typename ResponseT>
class Service {
public:
    Service() = default;

    explicit Service(std::shared_ptr<ServiceBase> base)
        : base_(std::move(base)) {}

    bool Valid() const {
        return static_cast<bool>(base_);
    }

private:
    std::shared_ptr<ServiceBase> base_;
};

}  // namespace openember