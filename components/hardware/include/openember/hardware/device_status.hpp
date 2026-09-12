#pragma once

#include <cstdint>
#include <string>

#include "openember/hardware/error.hpp"

namespace openember::hardware {

enum class DeviceAvailability {
    kUnspecified,
    kOffline,
    kOnline,
    kDegraded,
    kError,
};

enum class HealthState {
    kUnspecified,
    kOk,
    kWarning,
    kError,
    kStale,
};

struct DeviceStatus {
    DeviceAvailability availability = DeviceAvailability::kUnspecified;
    HealthState health = HealthState::kUnspecified;
    Error last_error;
    std::string message;
    std::uint64_t sequence = 0;
    std::uint64_t error_count = 0;
};

}  // namespace openember::hardware
