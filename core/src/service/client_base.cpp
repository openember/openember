#include "openember/client.hpp"

#include <utility>

namespace openember {

ClientBase::ClientBase(std::string service_name,
                       std::string request_type,
                       std::string response_type,
                       transport::ServiceClient raw_client)
    : service_name_(std::move(service_name)),
      request_type_(std::move(request_type)),
      response_type_(std::move(response_type)),
      raw_client_(std::move(raw_client)) {}

transport::Result<transport::ByteBuffer> ClientBase::CallRaw(
    const transport::ByteBuffer& request,
    std::chrono::milliseconds timeout) {
    return raw_client_.Call(request, timeout);
}

const std::string& ClientBase::ServiceName() const {
    return service_name_;
}

const std::string& ClientBase::RequestType() const {
    return request_type_;
}

const std::string& ClientBase::ResponseType() const {
    return response_type_;
}

}  // namespace openember