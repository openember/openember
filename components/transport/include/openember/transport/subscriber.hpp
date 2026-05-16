#pragma once

#include <functional>
#include <memory>

#include "openember/transport/message.hpp"

namespace openember::transport {

using MessageCallback = std::function<void(const Message&)>;

class Subscriber {
public:
    Subscriber();
    ~Subscriber();

    Subscriber(Subscriber&&) noexcept;
    Subscriber& operator=(Subscriber&&) noexcept;

    Subscriber(const Subscriber&) = delete;
    Subscriber& operator=(const Subscriber&) = delete;

private:
    friend class Session;

    class Impl;
    explicit Subscriber(std::unique_ptr<Impl> impl);

    std::unique_ptr<Impl> impl_;
};

}  // namespace openember::transport