#pragma once

#include <chrono>
#include <cstddef>
#include <string>

namespace openember::transport {

enum class ZenohMode {
    kPeer,
    kClient,
};

struct SessionOptions {
    ZenohMode mode = ZenohMode::kPeer;

    // Optional. Example:
    // tcp/127.0.0.1:7447
    std::string connect;

    // Optional. Example:
    // tcp/0.0.0.0:7447
    std::string listen;

    std::string robot_id = "default";
    std::string namespace_name = "";
};

struct TopicOptions {
    std::string name;
    std::size_t queue_size = 16;
};

struct ServiceOptions {
    std::string name;
    std::chrono::milliseconds default_timeout{1000};
};

}  // namespace openember::transport