#pragma once

#include <cstdint>
#include <memory>

#include "openember/osal/types.hpp"

namespace openember::osal {

class Semaphore {
public:
    explicit Semaphore(uint32_t initial_count = 0);
    ~Semaphore();

    Semaphore(const Semaphore&) = delete;
    Semaphore& operator=(const Semaphore&) = delete;
    Semaphore(Semaphore&&) = delete;
    Semaphore& operator=(Semaphore&&) = delete;

    Result post();
    Result wait();
    Result try_wait();
    Result wait_timeout_ms(int timeout_ms);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace openember::osal
