#pragma once

#include <memory>

#include "openember/osal/mutex.hpp"
#include "openember/osal/types.hpp"

namespace openember::osal {

class Cond {
public:
    Cond();
    ~Cond();

    Cond(const Cond&) = delete;
    Cond& operator=(const Cond&) = delete;
    Cond(Cond&&) = delete;
    Cond& operator=(Cond&&) = delete;

    Result wait(Mutex& mutex);
    Result wait_timeout_ms(Mutex& mutex, int timeout_ms);
    Result signal();
    Result broadcast();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace openember::osal
