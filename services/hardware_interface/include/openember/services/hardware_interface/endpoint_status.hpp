#pragma once

#include <cstdint>
#include <string>

namespace openember::services::hardware_interface {

enum class EndpointState {
    kCreated,
    kConfigured,
    kRunning,
    kStopping,
    kStopped,
    kError,
};

struct EndpointStatus {
    EndpointState state = EndpointState::kCreated;
    std::string endpoint_id;
    std::string device_id;
    std::string driver;
    std::string mode;
    std::string message;
    std::string last_error;
    std::uint64_t sequence = 0;
    std::uint64_t sample_count = 0;
    std::uint64_t publish_count = 0;
    std::uint64_t command_count = 0;
    std::uint64_t error_count = 0;
    std::uint64_t timeout_count = 0;
    std::uint64_t last_sample_monotonic_ns = 0;
    std::uint64_t last_publish_monotonic_ns = 0;
    std::uint64_t last_command_monotonic_ns = 0;
};

const char* ToString(EndpointState state) noexcept;

}  // namespace openember::services::hardware_interface
