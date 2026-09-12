#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "openember/actuator/actuator.hpp"

namespace openember::actuator {

enum class JointControlMode {
    kUnspecified,
    kDisabled,
    kPosition,
    kVelocity,
    kEffort,
    kMit,
};

struct JointInfo {
    std::uint32_t joint_id = 0;
    std::string joint_name;
    double min_position_rad = 0.0;
    double max_position_rad = 0.0;
    double max_velocity_radps = 0.0;
    double max_effort_nm = 0.0;
};

struct JointCommandItem {
    std::uint32_t joint_id = 0;
    std::string joint_name;
    JointControlMode mode = JointControlMode::kUnspecified;
    bool position_enabled = false;
    double position_rad = 0.0;
    bool velocity_enabled = false;
    double velocity_radps = 0.0;
    bool effort_enabled = false;
    double effort_nm = 0.0;
    bool stiffness_enabled = false;
    double stiffness_nm_per_rad = 0.0;
    bool damping_enabled = false;
    double damping_nms_per_rad = 0.0;
    double max_effort_nm = 0.0;
};

struct JointCommand {
    std::string controller_id;
    std::uint64_t command_time_monotonic_ns = 0;
    std::uint64_t sequence = 0;
    bool enable = false;
    bool disable = false;
    bool emergency_stop = false;
    bool clear_fault = false;
    std::vector<JointCommandItem> joints;
};

struct JointStateItem {
    std::uint32_t joint_id = 0;
    std::string joint_name;
    JointControlMode mode = JointControlMode::kDisabled;
    double position_rad = 0.0;
    double velocity_radps = 0.0;
    double effort_nm = 0.0;
    double temperature_celsius = 0.0;
    bool feedback_received = false;
    std::uint32_t fault_code = 0;
    std::string fault_message;
};

struct JointState {
    std::string controller_id;
    std::uint64_t sample_time_monotonic_ns = 0;
    std::uint64_t sequence = 0;
    ActuatorAvailability availability = ActuatorAvailability::kUnspecified;
    std::vector<JointStateItem> joints;
    std::string message;
};

}  // namespace openember::actuator
