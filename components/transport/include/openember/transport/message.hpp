#pragma once

#include <chrono>
#include <string>

#include "openember/transport/buffer.hpp"

namespace openember::transport {

struct Message {
    std::string key;
    ByteBuffer payload;
    std::chrono::steady_clock::time_point receive_time;
};

}  // namespace openember::transport