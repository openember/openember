#pragma once

#include <cstdint>
#include <string>

#include "openember/actuator/actuator_info.hpp"
#include "openember/hardware/device_status.hpp"
#include "openember/hardware/result.hpp"

namespace openember::actuator {

enum class ActuatorAvailability {
    kUnspecified,
    kOffline,
    kDisabled,
    kEnabled,
    kDegraded,
    kFault,
    kEmergencyStop,
};

struct ActuatorStatus {
    ActuatorAvailability availability = ActuatorAvailability::kUnspecified;
    hardware::HealthState health = hardware::HealthState::kUnspecified;
    hardware::Error last_error;
    std::string message;
    std::uint64_t sequence = 0;
    std::uint64_t error_count = 0;
    std::uint64_t timeout_count = 0;
    std::uint64_t last_command_monotonic_ns = 0;
    std::uint64_t command_timeout_ms = 0;
    bool enabled = false;
    bool fault_latched = false;
    bool emergency_stopped = false;
};

class IActuator {
public:
    virtual ~IActuator() = default;

    virtual const ActuatorInfo& Info() const = 0;
    virtual ActuatorStatus Status() const = 0;
    virtual hardware::Result<void> Start() = 0;
    virtual void Stop() noexcept = 0;
    virtual hardware::Result<void> Enable() = 0;
    virtual hardware::Result<void> Disable() = 0;
    virtual hardware::Result<void> EmergencyStop() = 0;
    virtual hardware::Result<void> ClearFault() = 0;
};

}  // namespace openember::actuator
