#pragma once

#include <chrono>
#include <memory>

#include "openember/transport/buffer.hpp"
#include "openember/transport/result.hpp"

namespace openember::transport {

class ServiceClient {
public:
    ServiceClient();
    ~ServiceClient();

    ServiceClient(ServiceClient&&) noexcept;
    ServiceClient& operator=(ServiceClient&&) noexcept;

    ServiceClient(const ServiceClient&) = delete;
    ServiceClient& operator=(const ServiceClient&) = delete;

    Result<ByteBuffer> Call(const ByteBuffer& request,
                            std::chrono::milliseconds timeout);

private:
    friend class Session;

    class Impl;
    explicit ServiceClient(std::unique_ptr<Impl> impl);

    std::unique_ptr<Impl> impl_;
};

}  // namespace openember::transport