#pragma once

#include <memory>

#include "openember/osal/types.hpp"

namespace openember::osal {

class Event {
public:
    Event();
    ~Event();

    Event(const Event&) = delete;
    Event& operator=(const Event&) = delete;
    Event(Event&&) = delete;
    Event& operator=(Event&&) = delete;

    Result set();
    Result reset();
    Result wait();
    Result wait_timeout_ms(int timeout_ms);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace openember::osal
