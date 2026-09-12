#include "openember/actuator/mock_joint_controller.hpp"

#include <algorithm>
#include <set>
#include <utility>

#include "openember/hardware/timestamp.hpp"

namespace openember::actuator {

namespace {

std::vector<JointInfo> DefaultJoints() {
    return {
        JointInfo{1, "joint_1", -3.14159, 3.14159, 10.0, 20.0},
        JointInfo{2, "joint_2", -3.14159, 3.14159, 10.0, 20.0},
        JointInfo{3, "joint_3", -3.14159, 3.14159, 10.0, 20.0},
    };
}

hardware::Error MakeActuatorError(hardware::ErrorCode code, const std::string& message) {
    return hardware::MakeError(code, message);
}

double ClampEffort(double effort, double command_limit, double joint_limit) {
    double limit = 0.0;
    if (command_limit > 0.0) {
        limit = command_limit;
    } else if (joint_limit > 0.0) {
        limit = joint_limit;
    }

    if (limit <= 0.0) {
        return effort;
    }
    return std::max(-limit, std::min(limit, effort));
}

}  // namespace

MockJointController::MockJointController(MockJointControllerOptions options)
    : options_(std::move(options)),
      joints_(options_.joints.empty() ? DefaultJoints() : options_.joints),
      last_step_(std::chrono::steady_clock::now()) {
    info_.actuator_id = options_.controller_id;
    info_.name = options_.controller_id;
    info_.type = ActuatorType::kJointController;
    info_.vendor = "OpenEmber";
    info_.model = "Mock JointController";
    info_.driver = "mock_joint_controller";
    info_.bus_type = ActuatorBusType::kVirtual;

    joint_states_.reserve(joints_.size());
    for (const auto& joint : joints_) {
        JointStateItem state;
        state.joint_id = joint.joint_id;
        state.joint_name = joint.joint_name;
        state.mode = JointControlMode::kDisabled;
        state.temperature_celsius = options_.default_temperature_celsius;
        state.feedback_received = true;
        joint_states_.push_back(std::move(state));
    }
}

MockJointController::~MockJointController() {
    Stop();
}

const ActuatorInfo& MockJointController::Info() const {
    return info_;
}

ActuatorStatus MockJointController::Status() const {
    std::lock_guard<std::mutex> lock(mutex_);
    ActuatorStatus status;
    status.availability = AvailabilityLocked();
    status.health = (fault_latched_ || emergency_stopped_)
        ? hardware::HealthState::kError
        : (running_ ? hardware::HealthState::kOk : hardware::HealthState::kStale);
    status.last_error = last_error_;
    status.message = message_;
    status.sequence = sequence_;
    status.error_count = error_count_;
    status.timeout_count = timeout_count_;
    status.last_command_monotonic_ns = last_command_monotonic_ns_;
    status.command_timeout_ms = options_.command_timeout_ms;
    status.enabled = enabled_;
    status.fault_latched = fault_latched_;
    status.emergency_stopped = emergency_stopped_;
    return status;
}

const std::vector<JointInfo>& MockJointController::Joints() const {
    return joints_;
}

hardware::Result<void> MockJointController::Start() {
    std::lock_guard<std::mutex> lock(mutex_);
    running_ = true;
    enabled_ = !options_.disabled_on_start;
    fault_latched_ = false;
    emergency_stopped_ = false;
    safe_output_active_ = !enabled_;
    has_command_ = false;
    message_ = enabled_ ? "mock joint controller running"
                       : "mock joint controller disabled";
    next_state_ = std::chrono::steady_clock::now();
    last_step_ = std::chrono::steady_clock::now();
    ++sequence_;
    cv_.notify_all();
    return hardware::Result<void>::Success();
}

void MockJointController::Stop() noexcept {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        running_ = false;
        enabled_ = false;
        safe_output_active_ = true;
        ApplySafeOutputLocked("mock joint controller stopped");
        ++sequence_;
    }
    cv_.notify_all();
}

hardware::Result<void> MockJointController::Enable() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!running_) {
        return hardware::Result<void>::Failure(
            MakeActuatorError(hardware::ErrorCode::kNotRunning, "mock joint controller is not running"));
    }
    if (fault_latched_) {
        return hardware::Result<void>::Failure(
            MakeActuatorError(hardware::ErrorCode::kSafetyViolation, "mock joint controller fault latched"));
    }
    if (emergency_stopped_) {
        return hardware::Result<void>::Failure(
            MakeActuatorError(hardware::ErrorCode::kSafetyViolation, "mock joint controller emergency stopped"));
    }

    enabled_ = true;
    safe_output_active_ = false;
    message_ = "mock joint controller enabled";
    ++sequence_;
    cv_.notify_all();
    return hardware::Result<void>::Success();
}

hardware::Result<void> MockJointController::Disable() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!running_) {
        return hardware::Result<void>::Failure(
            MakeActuatorError(hardware::ErrorCode::kNotRunning, "mock joint controller is not running"));
    }

    ApplySafeOutputLocked("mock joint controller disabled");
    ++sequence_;
    cv_.notify_all();
    return hardware::Result<void>::Success();
}

hardware::Result<void> MockJointController::EmergencyStop() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!running_) {
        return hardware::Result<void>::Failure(
            MakeActuatorError(hardware::ErrorCode::kNotRunning, "mock joint controller is not running"));
    }

    emergency_stopped_ = true;
    ApplySafeOutputLocked("mock joint controller emergency stopped");
    ++sequence_;
    cv_.notify_all();
    return hardware::Result<void>::Success();
}

hardware::Result<void> MockJointController::ClearFault() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!running_) {
        return hardware::Result<void>::Failure(
            MakeActuatorError(hardware::ErrorCode::kNotRunning, "mock joint controller is not running"));
    }

    fault_latched_ = false;
    emergency_stopped_ = false;
    last_error_ = {};
    for (auto& state : joint_states_) {
        state.fault_code = 0;
        state.fault_message.clear();
    }
    ApplySafeOutputLocked("mock joint controller fault cleared");
    ++sequence_;
    cv_.notify_all();
    return hardware::Result<void>::Success();
}

hardware::Result<void> MockJointController::SetCommand(const JointCommand& command) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!running_) {
        return hardware::Result<void>::Failure(
            MakeActuatorError(hardware::ErrorCode::kNotRunning, "mock joint controller is not running"));
    }

    if (!command.controller_id.empty() && command.controller_id != options_.controller_id) {
        return hardware::Result<void>::Failure(
            MakeActuatorError(hardware::ErrorCode::kInvalidData, "joint command controller_id mismatch"));
    }

    if (command.emergency_stop) {
        emergency_stopped_ = true;
        ApplySafeOutputLocked("mock joint controller emergency stopped");
        ++sequence_;
        cv_.notify_all();
        return hardware::Result<void>::Success();
    }

    if (command.clear_fault) {
        fault_latched_ = false;
        emergency_stopped_ = false;
        last_error_ = {};
        for (auto& state : joint_states_) {
            state.fault_code = 0;
            state.fault_message.clear();
        }
    }

    if (command.disable) {
        ApplySafeOutputLocked("mock joint controller disabled by command");
        ++sequence_;
        cv_.notify_all();
        return hardware::Result<void>::Success();
    }

    if (command.enable) {
        if (fault_latched_ || emergency_stopped_) {
            return hardware::Result<void>::Failure(
                MakeActuatorError(hardware::ErrorCode::kSafetyViolation,
                          "cannot enable mock joint controller while faulted"));
        }
        enabled_ = true;
        safe_output_active_ = false;
    }

    if (command.joints.empty()) {
        message_ = enabled_ ? "mock joint controller enabled"
                           : "mock joint controller command accepted";
        ++sequence_;
        cv_.notify_all();
        return hardware::Result<void>::Success();
    }

    if (!enabled_) {
        return hardware::Result<void>::Failure(
            MakeActuatorError(hardware::ErrorCode::kSafetyViolation, "mock joint controller is disabled"));
    }
    if (fault_latched_ || emergency_stopped_) {
        return hardware::Result<void>::Failure(
            MakeActuatorError(hardware::ErrorCode::kSafetyViolation, "mock joint controller is faulted"));
    }

    auto validation = ValidateCommandLocked(command);
    if (!validation.Ok()) {
        last_error_ = validation.Err();
        ++error_count_;
        message_ = validation.Err().message;
        return validation;
    }

    last_command_ = command;
    if (last_command_.controller_id.empty()) {
        last_command_.controller_id = options_.controller_id;
    }
    last_command_monotonic_ns_ = command.command_time_monotonic_ns != 0
        ? command.command_time_monotonic_ns
        : hardware::SteadyTimeNs();
    has_command_ = true;
    safe_output_active_ = false;
    message_ = "mock joint controller command accepted";
    ++sequence_;
    cv_.notify_all();
    return hardware::Result<void>::Success();
}

hardware::Result<void> MockJointController::Step() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!running_) {
        return hardware::Result<void>::Failure(
            MakeActuatorError(hardware::ErrorCode::kNotRunning, "mock joint controller is not running"));
    }

    const auto now = std::chrono::steady_clock::now();
    const double dt = std::chrono::duration<double>(now - last_step_).count();
    last_step_ = now;

    if (!enabled_) {
        ApplySafeOutputLocked(message_.empty() ? "mock joint controller disabled" : message_);
        ++sequence_;
        cv_.notify_all();
        return hardware::Result<void>::Success();
    }

    if (has_command_ && options_.command_timeout_ms > 0) {
        const auto now_ns = hardware::SteadyTimeNs();
        const auto timeout_ns = options_.command_timeout_ms * 1000000ULL;
        if (last_command_monotonic_ns_ != 0 &&
            now_ns > last_command_monotonic_ns_ + timeout_ns) {
            has_command_ = false;
            const std::string message =
                "mock joint controller command timeout; safe output active";
            ++timeout_count_;
            last_error_ = MakeActuatorError(hardware::ErrorCode::kTimeout, message);
            ApplySafeOutputLocked(message);
            ++sequence_;
            cv_.notify_all();
            return hardware::Result<void>::Success();
        }
    }

    if (!has_command_) {
        message_ = "mock joint controller waiting for command";
        ++sequence_;
        cv_.notify_all();
        return hardware::Result<void>::Success();
    }

    for (const auto& item : last_command_.joints) {
        const int index = FindJointIndex(item);
        if (index < 0) {
            continue;
        }

        auto& state = joint_states_[static_cast<std::size_t>(index)];
        const auto& joint = joints_[static_cast<std::size_t>(index)];
        state.mode = item.mode;
        state.feedback_received = true;
        state.temperature_celsius = options_.default_temperature_celsius;

        if (item.velocity_enabled) {
            state.velocity_radps = item.velocity_radps;
        } else if (item.mode == JointControlMode::kVelocity) {
            state.velocity_radps = 0.0;
        }

        if (item.mode == JointControlMode::kVelocity && item.velocity_enabled) {
            state.position_rad += item.velocity_radps * dt;
        }
        if (item.position_enabled) {
            state.position_rad = item.position_rad;
        }
        if (joint.min_position_rad < joint.max_position_rad) {
            state.position_rad = std::max(
                joint.min_position_rad,
                std::min(joint.max_position_rad, state.position_rad));
        }

        if (item.effort_enabled) {
            state.effort_nm =
                ClampEffort(item.effort_nm, item.max_effort_nm, joint.max_effort_nm);
        } else if (item.mode == JointControlMode::kEffort) {
            state.effort_nm = 0.0;
        }

        if (item.mode == JointControlMode::kDisabled) {
            state.velocity_radps = 0.0;
            state.effort_nm = 0.0;
        }
    }

    message_ = "mock joint controller running";
    safe_output_active_ = false;
    ++sequence_;
    cv_.notify_all();
    return hardware::Result<void>::Success();
}

hardware::Result<JointState> MockJointController::ReadState(
    std::chrono::milliseconds timeout) {
    std::unique_lock<std::mutex> lock(mutex_);
    if (!running_) {
        return hardware::Result<JointState>::Failure(
            MakeActuatorError(hardware::ErrorCode::kNotRunning, "mock joint controller is not running"));
    }

    const auto now = std::chrono::steady_clock::now();
    if (now < next_state_) {
        const auto wait_until = std::min(next_state_, now + timeout);
        cv_.wait_until(lock, wait_until, [&] { return !running_; });
        if (!running_) {
            return hardware::Result<JointState>::Failure(
                MakeActuatorError(hardware::ErrorCode::kNotRunning, "mock joint controller stopped"));
        }
        if (std::chrono::steady_clock::now() < next_state_) {
            return hardware::Result<JointState>::Failure(
                MakeActuatorError(hardware::ErrorCode::kTimeout, "mock joint controller read timeout"));
        }
    }

    JointState state;
    state.controller_id = options_.controller_id;
    state.sample_time_monotonic_ns = hardware::SteadyTimeNs();
    state.sequence = state_sequence_++;
    state.availability = AvailabilityLocked();
    state.joints = joint_states_;
    state.message = message_;
    next_state_ = std::chrono::steady_clock::now() + Period();
    return hardware::Result<JointState>::Success(std::move(state));
}

void MockJointController::InjectFault(std::string message, std::uint32_t fault_code) {
    std::lock_guard<std::mutex> lock(mutex_);
    fault_latched_ = true;
    last_error_ = MakeActuatorError(hardware::ErrorCode::kSafetyViolation, message);
    ++error_count_;
    for (auto& state : joint_states_) {
        state.fault_code = fault_code;
        state.fault_message = message;
    }
    ApplySafeOutputLocked(std::move(message));
    ++sequence_;
    cv_.notify_all();
}

std::chrono::steady_clock::duration MockJointController::Period() const {
    const auto hz = std::max(options_.state_rate_hz, 0.001);
    return std::chrono::duration_cast<std::chrono::steady_clock::duration>(
        std::chrono::duration<double>(1.0 / hz));
}

int MockJointController::FindJointIndex(const JointCommandItem& item) const {
    for (std::size_t i = 0; i < joints_.size(); ++i) {
        const auto& joint = joints_[i];
        if (item.joint_id != 0 && item.joint_id == joint.joint_id) {
            return static_cast<int>(i);
        }
        if (!item.joint_name.empty() && item.joint_name == joint.joint_name) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

hardware::Result<void> MockJointController::ValidateCommandLocked(
    const JointCommand& command) const {
    std::set<int> seen_joints;

    for (const auto& item : command.joints) {
        const int index = FindJointIndex(item);
        if (index < 0) {
            return hardware::Result<void>::Failure(
                MakeActuatorError(hardware::ErrorCode::kInvalidData, "unknown joint in command"));
        }
        if (!seen_joints.insert(index).second) {
            return hardware::Result<void>::Failure(
                MakeActuatorError(hardware::ErrorCode::kInvalidData, "duplicate joint in command"));
        }

        if (item.mode == JointControlMode::kUnspecified) {
            return hardware::Result<void>::Failure(
                MakeActuatorError(hardware::ErrorCode::kInvalidData, "joint control mode is unspecified"));
        }

        const bool has_control =
            item.position_enabled || item.velocity_enabled || item.effort_enabled ||
            item.stiffness_enabled || item.damping_enabled;
        if (item.mode != JointControlMode::kDisabled && !has_control) {
            return hardware::Result<void>::Failure(
                MakeActuatorError(hardware::ErrorCode::kInvalidData, "joint command has no enabled field"));
        }
        if (item.max_effort_nm < 0.0 ||
            item.stiffness_nm_per_rad < 0.0 ||
            item.damping_nms_per_rad < 0.0) {
            return hardware::Result<void>::Failure(
                MakeActuatorError(hardware::ErrorCode::kInvalidData, "joint command limit must be >= 0"));
        }
    }

    return hardware::Result<void>::Success();
}

void MockJointController::ApplySafeOutputLocked(const std::string& message) {
    enabled_ = false;
    safe_output_active_ = true;
    message_ = message;
    for (auto& state : joint_states_) {
        state.mode = JointControlMode::kDisabled;
        state.velocity_radps = 0.0;
        state.effort_nm = 0.0;
    }
}

ActuatorAvailability MockJointController::AvailabilityLocked() const {
    if (!running_) {
        return ActuatorAvailability::kOffline;
    }
    if (emergency_stopped_) {
        return ActuatorAvailability::kEmergencyStop;
    }
    if (fault_latched_) {
        return ActuatorAvailability::kFault;
    }
    if (safe_output_active_ && enabled_) {
        return ActuatorAvailability::kDegraded;
    }
    return enabled_ ? ActuatorAvailability::kEnabled
                    : ActuatorAvailability::kDisabled;
}

}  // namespace openember::actuator
