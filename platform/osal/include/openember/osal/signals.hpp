#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

#include "openember/osal/types.hpp"

namespace openember::osal {

struct SignalsCaps {
    uint32_t supports_signalfd = 0;
};

struct SignalInfo {
    int signum = 0;
    int sender_pid = 0;
};

class SignalWaiter {
public:
    SignalWaiter();
    ~SignalWaiter();

    SignalWaiter(const SignalWaiter&) = delete;
    SignalWaiter& operator=(const SignalWaiter&) = delete;
    SignalWaiter(SignalWaiter&&) = delete;
    SignalWaiter& operator=(SignalWaiter&&) = delete;

    static Result query_caps(SignalsCaps* out_caps);

    Result open(const std::vector<int>& signums);
    Result wait(SignalInfo* out_info, int timeout_ms);
    Result close();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace openember::osal
