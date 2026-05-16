#include "openember/service.hpp"

#include <utility>

namespace openember {

ServiceBase::ServiceBase(std::string service_name,
                         std::string request_type,
                         std::string response_type,
                         transport::ServiceServer raw_server)
    : service_name_(std::move(service_name)),
      request_type_(std::move(request_type)),
      response_type_(std::move(response_type)),
      raw_server_(std::move(raw_server)) {}

const std::string& ServiceBase::ServiceName() const {
    return service_name_;
}

const std::string& ServiceBase::RequestType() const {
    return request_type_;
}

const std::string& ServiceBase::ResponseType() const {
    return response_type_;
}

}  // namespace openember