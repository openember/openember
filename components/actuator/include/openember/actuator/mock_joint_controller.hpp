#pragma once

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

#include "openember/actuator/joint_controller.hpp"

namespace openember::actuator {

struct MockJointControllerOptions {
    std::string controller_id = "joint_controller0";
    double state_rate_hz = 100.0;
    std::uint64_t command_timeout_ms = 100;
    bool disabled_on_start = true;
    double default_temperature_celsius = 30.0;
    std::vector<JointInfo> joints;
};

class MockJointController final : public IJointController {
public:
    explicit MockJointController(MockJointControllerOptions options = {});
    ~MockJointController() override;

    const ActuatorInfo& Info() const override;
    ActuatorStatus Status() const override;
    const std::vector<JointInfo>& Joints() const override;

    hardware::Result<void> Start() override;
    void Stop() noexcept override;
    hardware::Result<void> Enable() override;
    hardware::Result<void> Disable() override;
    hardware::Result<void> EmergencyStop() override;
    hardware::Result<void> ClearFault() override;

    hardware::Result<void> SetCommand(const JointCommand& command) override;
    hardware::Result<void> Step() override;
    hardware::Result<JointState> ReadState(
        std::chrono::milliseconds timeout) override;

    void InjectFault(std::string message, std::uint32_t fault_code = 1);

private:
    std::chrono::steady_clock::duration Period() const;
    int FindJointIndex(const JointCommandItem& item) const;
    hardware::Result<void> ValidateCommandLocked(
        const JointCommand& command) const;
    void ApplySafeOutputLocked(const std::string& message);
    ActuatorAvailability AvailabilityLocked() const;

    MockJointControllerOptions options_;
    ActuatorInfo info_;
    std::vector<JointInfo> joints_;
    std::vector<JointStateItem> joint_states_;
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    bool running_ = false;
    bool enabled_ = false;
    bool fault_latched_ = false;
    bool emergency_stopped_ = false;
    bool has_command_ = false;
    bool safe_output_active_ = false;
    JointCommand last_command_;
    hardware::Error last_error_;
    std::string message_ = "created";
    std::uint64_t sequence_ = 0;
    std::uint64_t state_sequence_ = 0;
    std::uint64_t error_count_ = 0;
    std::uint64_t timeout_count_ = 0;
    std::uint64_t last_command_monotonic_ns_ = 0;
    std::chrono::steady_clock::time_point next_state_;
    std::chrono::steady_clock::time_point last_step_;
};

}  // namespace openember::actuator
