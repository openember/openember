#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace openember::actuator::control {

struct JointFeedback {
    std::string joint_name;
    std::uint64_t sample_sequence = 0, receive_time_ns = 0;
    bool receive_fresh = false;
    double position_rad = 0, velocity_radps = 0, effort_nm = 0;
    double temperature_celsius = 0, voltage_v = 0;
    bool position_valid = false, velocity_valid = false, effort_valid = false;
    bool temperature_valid = false, voltage_valid = false;
    std::uint32_t fault_code = 0;
    bool fault_valid = false;
};

// Owner supplies actual per-joint sample metadata. Publication does not sample.
struct FeedbackFrame {
    std::string controller_session, clock_domain;
    std::uint64_t sequence = 0, publish_time_ns = 0;
    std::vector<JointFeedback> joints;
    std::uint64_t physics_step = 0, simulation_time_ns = 0;
    bool paused = false;
    std::string message;
};

}  // namespace openember::actuator::control
