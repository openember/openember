#pragma once

#include <functional>
#include <memory>

#include "openember/transport/buffer.hpp"
#include "openember/transport/message.hpp"
#include "openember/transport/result.hpp"

namespace openember::transport {

using ServiceCallback = std::function<Result<ByteBuffer>(const Message& request)>;

class ServiceServer {
public:
    ServiceServer();
    ~ServiceServer();

    ServiceServer(ServiceServer&&) noexcept;
    ServiceServer& operator=(ServiceServer&&) noexcept;

    ServiceServer(const ServiceServer&) = delete;
    ServiceServer& operator=(const ServiceServer&) = delete;

private:
    friend class Session;

    class Impl;
    explicit ServiceServer(std::unique_ptr<Impl> impl);

    std::unique_ptr<Impl> impl_;
};

}  // namespace openember::transport