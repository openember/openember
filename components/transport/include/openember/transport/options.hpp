#pragma once

#include <chrono>
#include <cstddef>
#include <string>

namespace openember::transport {

enum class ZenohMode {
    kPeer,
    kClient,
    // 本机 TCP listen（如 transport_listener）：zenoh-c 需 router + listen。
    kRouter,
};

// 本机示例（transport_talker / transport_listener）共用的 Zenoh TCP 端口。
inline constexpr const char* kDefaultZenohListenEndpoint = "tcp/127.0.0.1:7447";

struct SessionOptions {
    ZenohMode mode = ZenohMode::kPeer;

    // Zenoh locator，会写入 connect/endpoints（JSON 数组）。示例：tcp/127.0.0.1:7447
    std::string connect;

    // Zenoh locator，会写入 listen/endpoints（JSON 数组）。示例：tcp/127.0.0.1:7447
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