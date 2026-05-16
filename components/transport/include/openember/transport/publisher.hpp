#pragma once

#include <memory>

#include "openember/transport/buffer.hpp"
#include "openember/transport/result.hpp"

namespace openember::transport {

class Publisher {
public:
    Publisher();
    ~Publisher();

    Publisher(Publisher&&) noexcept;
    Publisher& operator=(Publisher&&) noexcept;

    Publisher(const Publisher&) = delete;
    Publisher& operator=(const Publisher&) = delete;

    Result<void> Publish(const ByteBuffer& payload);

private:
    friend class Session;

    class Impl;
    explicit Publisher(std::unique_ptr<Impl> impl);

    std::unique_ptr<Impl> impl_;
};

}  // namespace openember::transport