#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

#include "openember/osal/types.hpp"

namespace openember::hal {

using Result = osal::Result;

struct CanConfig {
    uint32_t enable_can_fd = 0;
};

struct CanFrame {
    uint32_t can_id = 0;
    uint8_t data_len = 0;
    uint8_t is_extended = 0;
    uint8_t is_fd = 0;
    uint8_t data[64] {};
};

struct CanCaps {
    uint32_t supports_can_fd = 0;
    uint32_t max_data_len_can = 0;
    uint32_t max_data_len_can_fd = 0;
};

class Can {
public:
    Can();
    ~Can();

    Can(const Can&) = delete;
    Can& operator=(const Can&) = delete;
    Can(Can&&) = delete;
    Can& operator=(Can&&) = delete;

    static Result query_caps(CanCaps* out_caps);

    Result open(const std::string& ifname, const CanConfig& cfg);
    Result close();
    Result send_frame(const CanFrame& frame);
    Result recv_frame(CanFrame* out_frame, int timeout_ms);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace openember::hal
