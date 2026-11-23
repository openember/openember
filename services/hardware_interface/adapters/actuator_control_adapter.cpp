#include "openember/services/hardware_interface/actuator_control_adapter.hpp"

#include <algorithm>
#include <cmath>
#include <set>

namespace openember::services::hardware_interface::actuator_control {
namespace {
using hardware::ErrorCode;
using Mode = openember::actuator::JointControlMode;

template <typename T> Result<T> Bad(const char* message, ErrorCode code = ErrorCode::kInvalidData) {
    return Result<T>::Failure(hardware::MakeError(code, message));
}
bool Id(const std::string& value, std::size_t maximum = 63) {
    return !value.empty() && value.size() <= maximum &&
        std::all_of(value.begin(), value.end(), [](unsigned char c) { return c > 0 && c < 128; });
}
bool ContextShape(const wire::CommandContext& c) {
    return Id(c.robot_id()) && Id(c.controller_id()) && Id(c.controller_session()) &&
        Id(c.model_revision(), 127) && Id(c.producer_session()) && Id(c.clock_domain(), 127) &&
        c.sequence() && c.issued_time_ns() && c.expires_time_ns() > c.issued_time_ns();
}
domain::Context ContextIn(const wire::CommandContext& c) {
    return {c.robot_id(), c.controller_id(), c.controller_session(), c.model_revision(),
            c.producer_session(), c.sequence(), c.clock_domain(), c.issued_time_ns(), c.expires_time_ns()};
}
void ContextOut(const domain::Context& c, wire::CommandContext* out) {
    out->set_robot_id(c.robot_id); out->set_controller_id(c.controller_id);
    out->set_controller_session(c.controller_session); out->set_model_revision(c.model_revision);
    out->set_producer_session(c.producer_session); out->set_sequence(c.sequence);
    out->set_clock_domain(c.clock_domain); out->set_issued_time_ns(c.issued_time_ns);
    out->set_expires_time_ns(c.expires_time_ns);
}
bool LeaseShape(const wire::ControlLease& lease, bool required) {
    return required ? lease.generation() && Id(lease.token()) : !lease.generation() && lease.token().empty();
}
bool ModeIn(wire::JointControlMode value, Mode& result) {
    switch (value) {
        case wire::JOINT_CONTROL_MODE_DISABLED: result = Mode::kDisabled; return true;
        case wire::JOINT_CONTROL_MODE_POSITION: result = Mode::kPosition; return true;
        case wire::JOINT_CONTROL_MODE_VELOCITY: result = Mode::kVelocity; return true;
        case wire::JOINT_CONTROL_MODE_EFFORT: result = Mode::kEffort; return true;
        case wire::JOINT_CONTROL_MODE_MIT: result = Mode::kMit; return true;
        default: return false;
    }
}
bool ModeOut(Mode value, wire::JointControlMode& result) {
    switch (value) {
        case Mode::kDisabled: result = wire::JOINT_CONTROL_MODE_DISABLED; return true;
        case Mode::kPosition: result = wire::JOINT_CONTROL_MODE_POSITION; return true;
        case Mode::kVelocity: result = wire::JOINT_CONTROL_MODE_VELOCITY; return true;
        case Mode::kEffort: result = wire::JOINT_CONTROL_MODE_EFFORT; return true;
        case Mode::kMit: result = wire::JOINT_CONTROL_MODE_MIT; return true;
        default: return false;
    }
}
bool OperationIn(wire::ControlOperation value, domain::Operation& result) {
    switch (value) {
        case wire::CONTROL_OPERATION_ACQUIRE: result = domain::Operation::kAcquire; return true;
        case wire::CONTROL_OPERATION_RENEW: result = domain::Operation::kRenew; return true;
        case wire::CONTROL_OPERATION_RELEASE: result = domain::Operation::kRelease; return true;
        case wire::CONTROL_OPERATION_ARM: result = domain::Operation::kArm; return true;
        case wire::CONTROL_OPERATION_DISARM: result = domain::Operation::kDisarm; return true;
        case wire::CONTROL_OPERATION_EMERGENCY_STOP: result = domain::Operation::kEmergencyStop; return true;
        case wire::CONTROL_OPERATION_CLEAR_FAULT: result = domain::Operation::kClearFault; return true;
        default: return false;
    }
}
bool OperationOut(domain::Operation value, wire::ControlOperation& result) {
    switch (value) {
        case domain::Operation::kAcquire: result = wire::CONTROL_OPERATION_ACQUIRE; return true;
        case domain::Operation::kRenew: result = wire::CONTROL_OPERATION_RENEW; return true;
        case domain::Operation::kRelease: result = wire::CONTROL_OPERATION_RELEASE; return true;
        case domain::Operation::kArm: result = wire::CONTROL_OPERATION_ARM; return true;
        case domain::Operation::kDisarm: result = wire::CONTROL_OPERATION_DISARM; return true;
        case domain::Operation::kEmergencyStop: result = wire::CONTROL_OPERATION_EMERGENCY_STOP; return true;
        case domain::Operation::kClearFault: result = wire::CONTROL_OPERATION_CLEAR_FAULT; return true;
        default: return false;
    }
}
bool StateOut(domain::State value, wire::ControllerState& result) {
    switch (value) {
        case domain::State::kDisarmed: result = wire::CONTROLLER_STATE_DISARMED; return true;
        case domain::State::kArmed: result = wire::CONTROLLER_STATE_ARMED; return true;
        case domain::State::kFault: result = wire::CONTROLLER_STATE_FAULT; return true;
        case domain::State::kEmergencyStop: result = wire::CONTROLLER_STATE_EMERGENCY_STOP; return true;
        case domain::State::kStopped: result = wire::CONTROLLER_STATE_STOPPED; return true;
        default: return false;
    }
}
bool ReasonOut(domain::Reason value, wire::RejectionReason& result) {
    switch (value) {
        case domain::Reason::kNone: result = wire::REJECTION_REASON_UNSPECIFIED; return true;
        case domain::Reason::kInvalidArgument: result = wire::REJECTION_REASON_INVALID_ARGUMENT; return true;
        case domain::Reason::kWrongContext: result = wire::REJECTION_REASON_WRONG_CONTEXT; return true;
        case domain::Reason::kClockMismatch: result = wire::REJECTION_REASON_CLOCK_MISMATCH; return true;
        case domain::Reason::kExpired: result = wire::REJECTION_REASON_EXPIRED; return true;
        case domain::Reason::kReplay: result = wire::REJECTION_REASON_REPLAY; return true;
        case domain::Reason::kNotOwner: result = wire::REJECTION_REASON_NOT_OWNER; return true;
        case domain::Reason::kBusy: result = wire::REJECTION_REASON_BUSY; return true;
        case domain::Reason::kInvalidState: result = wire::REJECTION_REASON_INVALID_STATE; return true;
        case domain::Reason::kTopologyMismatch: result = wire::REJECTION_REASON_TOPOLOGY_MISMATCH; return true;
        case domain::Reason::kUnsupportedMode: result = wire::REJECTION_REASON_UNSUPPORTED_MODE; return true;
        case domain::Reason::kLimitViolation: result = wire::REJECTION_REASON_LIMIT_VIOLATION; return true;
        case domain::Reason::kBackendFailure: result = wire::REJECTION_REASON_BACKEND_FAILURE; return true;
        default: return false;
    }
}
}  // namespace

Result<domain::Command> FromProto(const wire::JointCommand& message) {
    if (!message.has_context() || !ContextShape(message.context()) || !message.has_lease() ||
        !LeaseShape(message.lease(), true) || message.joints_size() < 1 || message.joints_size() > 32)
        return Bad<domain::Command>("invalid v2 command context, lease or joint count");
    domain::Command out;
    out.context = ContextIn(message.context());
    out.lease = {message.lease().generation(), message.lease().token()};
    std::set<std::string> names;
    for (const auto& joint : message.joints()) {
        if (!Id(joint.joint_name()) || !names.insert(joint.joint_name()).second)
            return Bad<domain::Command>("invalid or duplicate v2 joint name");
        domain::Target target;
        if (!ModeIn(joint.mode(), target.mode))
            return Bad<domain::Command>("unknown or unspecified v2 joint mode", ErrorCode::kNotSupported);
        for (double value : {joint.position_rad(), joint.velocity_radps(), joint.effort_nm(),
                            joint.stiffness_nm_per_rad(), joint.damping_nms_per_rad(), joint.max_effort_nm()})
            if (!std::isfinite(value)) return Bad<domain::Command>("non-finite v2 target value");
        target.joint_name = joint.joint_name(); target.position_rad = joint.position_rad();
        target.velocity_radps = joint.velocity_radps(); target.effort_nm = joint.effort_nm();
        target.stiffness_nm_per_rad = joint.stiffness_nm_per_rad();
        target.damping_nms_per_rad = joint.damping_nms_per_rad(); target.max_effort_nm = joint.max_effort_nm();
        out.joints.push_back(std::move(target));
    }
    return Result<domain::Command>::Success(std::move(out));
}

Result<domain::Request> FromProto(const wire::ControlRequest& message) {
    if (!message.has_context() || !ContextShape(message.context()) || !Id(message.request_id()))
        return Bad<domain::Request>("invalid v2 management context or request ID");
    domain::Request out;
    if (!OperationIn(message.operation(), out.operation))
        return Bad<domain::Request>("unknown or unspecified v2 operation");
    const bool leased = out.operation == domain::Operation::kRenew || out.operation == domain::Operation::kRelease ||
                        out.operation == domain::Operation::kArm;
    const bool duration = out.operation == domain::Operation::kAcquire || out.operation == domain::Operation::kRenew;
    if ((leased && !message.has_lease()) || !LeaseShape(message.lease(), leased) ||
        (duration ? !message.requested_lease_duration_ns() : message.requested_lease_duration_ns() != 0))
        return Bad<domain::Request>("invalid v2 management lease or duration");
    out.context = ContextIn(message.context()); out.request_id = message.request_id();
    out.lease = {message.lease().generation(), message.lease().token()};
    out.requested_lease_duration_ns = message.requested_lease_duration_ns();
    return Result<domain::Request>::Success(std::move(out));
}

Result<domain::Capabilities> FromProto(const wire::ControllerCapabilities& message) {
    // Check bounds before copying attacker-controlled repeated/string fields.
    if (message.joints_size() < 1 || message.joints_size() > 32 || !Id(message.robot_id()) ||
        !Id(message.controller_id()) || !Id(message.controller_session()) ||
        !Id(message.model_revision(), 127) || !Id(message.clock_domain(), 127))
        return Bad<domain::Capabilities>("invalid v2 capabilities bounds");
    domain::Capabilities out;
    out.contract_version = message.contract_version(); out.robot_id = message.robot_id();
    out.controller_id = message.controller_id(); out.controller_session = message.controller_session();
    out.model_revision = message.model_revision(); out.clock_domain = message.clock_domain();
    out.simulation = message.simulation(); out.max_command_duration_ns = message.max_command_duration_ns();
    out.max_lease_duration_ns = message.max_lease_duration_ns(); out.command_timeout_ns = message.command_timeout_ns();
    for (const auto& joint : message.joints()) {
        if (!Id(joint.joint_name()) || joint.supported_modes_size() < 1 || joint.supported_modes_size() > 5)
            return Bad<domain::Capabilities>("invalid v2 joint capability bounds");
        domain::JointCapability c;
        c.joint_name = joint.joint_name(); c.continuous = joint.continuous();
        c.min_position_rad = joint.min_position_rad(); c.max_position_rad = joint.max_position_rad();
        c.max_velocity_radps = joint.max_velocity_radps(); c.max_effort_nm = joint.max_effort_nm();
        c.max_stiffness_nm_per_rad = joint.max_stiffness_nm_per_rad();
        c.max_damping_nms_per_rad = joint.max_damping_nms_per_rad(); c.max_feedback_age_ns = joint.max_feedback_age_ns();
        c.hardware_effort_limit_supported = joint.hardware_effort_limit_supported();
        for (int i = 0; i < joint.supported_modes_size(); ++i) {
            Mode mode;
            if (!ModeIn(joint.supported_modes(i), mode))
                return Bad<domain::Capabilities>("unknown v2 capability mode", ErrorCode::kNotSupported);
            c.supported_modes.push_back(mode);
        }
        out.joints.push_back(std::move(c));
    }
    if (!domain::ValidateCapabilities(out).Ok()) return Bad<domain::Capabilities>("invalid v2 capabilities");
    return Result<domain::Capabilities>::Success(std::move(out));
}

Result<wire::ControllerCapabilities> ToProto(const domain::Capabilities& c) {
    if (!domain::ValidateCapabilities(c).Ok()) return Bad<wire::ControllerCapabilities>("invalid domain capabilities");
    wire::ControllerCapabilities out;
    out.set_contract_version(c.contract_version); out.set_robot_id(c.robot_id); out.set_controller_id(c.controller_id);
    out.set_controller_session(c.controller_session); out.set_model_revision(c.model_revision); out.set_clock_domain(c.clock_domain);
    out.set_simulation(c.simulation); out.set_max_command_duration_ns(c.max_command_duration_ns);
    out.set_max_lease_duration_ns(c.max_lease_duration_ns); out.set_command_timeout_ns(c.command_timeout_ns);
    for (const auto& j : c.joints) {
        auto* joint = out.add_joints();
        joint->set_joint_name(j.joint_name); joint->set_continuous(j.continuous);
        joint->set_min_position_rad(j.min_position_rad); joint->set_max_position_rad(j.max_position_rad);
        joint->set_max_velocity_radps(j.max_velocity_radps); joint->set_max_effort_nm(j.max_effort_nm);
        joint->set_max_stiffness_nm_per_rad(j.max_stiffness_nm_per_rad);
        joint->set_max_damping_nms_per_rad(j.max_damping_nms_per_rad); joint->set_max_feedback_age_ns(j.max_feedback_age_ns);
        joint->set_hardware_effort_limit_supported(j.hardware_effort_limit_supported);
        for (auto mode : j.supported_modes) {
            wire::JointControlMode encoded;
            if (!ModeOut(mode, encoded)) return Bad<wire::ControllerCapabilities>("unknown domain joint mode");
            joint->add_supported_modes(encoded);
        }
    }
    return Result<wire::ControllerCapabilities>::Success(std::move(out));
}

Result<wire::ControlResult> ToProto(const domain::Capabilities& c, const domain::Request& request,
                                  const domain::ControlResult& result) {
    if (!domain::ValidateCapabilities(c).Ok()) return Bad<wire::ControlResult>("invalid result capabilities");
    wire::ControlRequest checked;
    ContextOut(request.context, checked.mutable_context()); checked.set_request_id(request.request_id);
    wire::ControlOperation operation;
    if (!OperationOut(request.operation, operation)) return Bad<wire::ControlResult>("unknown domain operation");
    checked.set_operation(operation); checked.set_requested_lease_duration_ns(request.requested_lease_duration_ns);
    checked.mutable_lease()->set_generation(request.lease.generation); checked.mutable_lease()->set_token(request.lease.token);
    if (!FromProto(checked).Ok()) return Bad<wire::ControlResult>("invalid result request association");
    wire::ControllerState state; wire::RejectionReason reason;
    if (!StateOut(result.state, state) || !ReasonOut(result.reason, reason))
        return Bad<wire::ControlResult>("unknown domain result state or reason");
    wire::ControlResult out;
    switch (result.code) {
        case domain::ResultCode::kCompleted:
            if (result.reason != domain::Reason::kNone) return Bad<wire::ControlResult>("completed result with rejection reason");
            out.set_code(wire::CONTROL_RESULT_CODE_COMPLETED); break;
        case domain::ResultCode::kRejected:
            if (result.reason == domain::Reason::kNone) return Bad<wire::ControlResult>("rejection without reason");
            out.set_code(wire::CONTROL_RESULT_CODE_REJECTED); break;
        case domain::ResultCode::kFailed:
            if (result.reason == domain::Reason::kNone) return Bad<wire::ControlResult>("failure without reason");
            out.set_code(wire::CONTROL_RESULT_CODE_FAILED); break;
        default: return Bad<wire::ControlResult>("unknown domain result code");
    }
    const bool grant = result.code == domain::ResultCode::kCompleted &&
        (request.operation == domain::Operation::kAcquire || request.operation == domain::Operation::kRenew);
    if (grant) {
        if (!result.lease.generation || !Id(result.lease.token) || !result.lease_expires_time_ns)
            return Bad<wire::ControlResult>("invalid completed lease grant");
        out.mutable_lease()->set_generation(result.lease.generation); out.mutable_lease()->set_token(result.lease.token);
        out.set_lease_expires_time_ns(result.lease_expires_time_ns);
    } else if (result.lease.generation || !result.lease.token.empty() || result.lease_expires_time_ns) {
        return Bad<wire::ControlResult>("lease disclosure in non-grant result");
    }
    // Response authority is always the receiver, even when the request context was wrong.
    out.set_robot_id(c.robot_id); out.set_controller_id(c.controller_id); out.set_controller_session(c.controller_session);
    out.set_producer_session(request.context.producer_session); out.set_request_id(request.request_id);
    out.set_request_sequence(request.context.sequence); out.set_operation(operation); out.set_state(state); out.set_reason(reason);
    out.set_message(result.code == domain::ResultCode::kCompleted ? "completed" :
                    result.code == domain::ResultCode::kRejected ? "rejected" : "backend or session operation failed");
    return Result<wire::ControlResult>::Success(std::move(out));
}

Result<wire::ControllerFeedback> ToProto(const domain::Capabilities& c, const domain::Status& s,
                                       const domain::FeedbackFrame& frame) {
    if (!domain::ValidateCapabilities(c).Ok() || !frame.sequence || !frame.publish_time_ns ||
        frame.controller_session != c.controller_session || frame.clock_domain != c.clock_domain || frame.paused != s.paused ||
        frame.message.size() > 159 || frame.message.find('\0') != std::string::npos ||
        frame.joints.size() != c.joints.size() ||
        (!c.simulation && (s.paused || frame.physics_step || frame.simulation_time_ns)) ||
        (c.simulation && ((!frame.physics_step) != (!frame.simulation_time_ns))))
        return Bad<wire::ControllerFeedback>("invalid v2 feedback frame or epoch");
    wire::ControllerState state; wire::RejectionReason rejection, fault;
    if (!StateOut(s.state, state) || !ReasonOut(s.last_rejection_reason, rejection) || !ReasonOut(s.fault_reason, fault))
        return Bad<wire::ControllerFeedback>("unknown domain feedback state or reason");
    if ((!s.owner_generation && (!s.owner_producer_session.empty() || s.lease_expires_time_ns ||
            s.last_accepted_command_sequence || s.last_applied_command_sequence || s.last_accepted_command_time_ns)) ||
        (s.owner_generation && (!Id(s.owner_producer_session) || s.lease_expires_time_ns <= frame.publish_time_ns)) ||
        (s.state == domain::State::kArmed && (!s.owner_generation || s.paused)) ||
        ((s.state == domain::State::kFault || s.state == domain::State::kEmergencyStop ||
          s.state == domain::State::kStopped) && s.owner_generation) ||
        s.last_applied_command_sequence > s.last_accepted_command_sequence ||
        (!s.last_accepted_command_sequence && s.last_accepted_command_time_ns) ||
        (s.last_accepted_command_sequence && (s.state != domain::State::kArmed || !s.last_accepted_command_time_ns ||
                                             s.last_accepted_command_time_ns > frame.publish_time_ns)))
        return Bad<wire::ControllerFeedback>("inconsistent v2 feedback authority or command cursors");
    wire::ControllerFeedback out;
    out.set_robot_id(c.robot_id); out.set_controller_id(c.controller_id); out.set_controller_session(c.controller_session);
    out.set_model_revision(c.model_revision); out.set_clock_domain(c.clock_domain);
    out.set_sequence(frame.sequence); out.set_publish_time_ns(frame.publish_time_ns); out.set_state(state);
    out.set_owner_generation(s.owner_generation); out.set_owner_producer_session(s.owner_producer_session);
    out.set_lease_expires_time_ns(s.lease_expires_time_ns);
    out.set_last_accepted_command_sequence(s.last_accepted_command_sequence);
    out.set_last_applied_command_sequence(s.last_applied_command_sequence);
    out.set_last_accepted_command_time_ns(s.last_accepted_command_time_ns);
    out.set_rejected_command_count(s.rejected_command_count); out.set_last_rejection_reason(rejection);
    out.set_backend_failure(s.backend_failure); out.set_fault_reason(fault);
    out.set_simulation(c.simulation); out.set_paused(s.paused); out.set_physics_step(frame.physics_step);
    out.set_simulation_time_ns(frame.simulation_time_ns); out.set_message(frame.message);
    std::set<std::string> names;
    for (const auto& j : frame.joints) {
        const auto cap = std::find_if(c.joints.begin(), c.joints.end(), [&](const auto& item) { return item.joint_name == j.joint_name; });
        if (!Id(j.joint_name) || !names.insert(j.joint_name).second || cap == c.joints.end() ||
            (c.simulation && !frame.physics_step && j.sample_sequence) ||
            (!j.sample_sequence && (j.receive_time_ns || j.receive_fresh || j.position_valid || j.velocity_valid ||
                                   j.effort_valid || j.temperature_valid || j.voltage_valid || j.fault_valid)) ||
            (j.sample_sequence && (!j.receive_time_ns || j.receive_time_ns > frame.publish_time_ns)))
            return Bad<wire::ControllerFeedback>("invalid v2 per-joint sample metadata");
        for (auto channel : {std::pair{j.position_valid, j.position_rad}, std::pair{j.velocity_valid, j.velocity_radps},
                             std::pair{j.effort_valid, j.effort_nm}, std::pair{j.temperature_valid, j.temperature_celsius},
                             std::pair{j.voltage_valid, j.voltage_v}})
            if (channel.first && !std::isfinite(channel.second)) return Bad<wire::ControllerFeedback>("non-finite valid feedback channel");
        auto* joint = out.add_joints(); joint->set_joint_name(j.joint_name); joint->set_sample_sequence(j.sample_sequence);
        joint->set_receive_time_ns(j.receive_time_ns);
        joint->set_receive_fresh(domain::SampleFresh(j.sample_sequence, j.receive_time_ns, j.receive_fresh,
                                                    cap->max_feedback_age_ns, frame.publish_time_ns));
        joint->set_position_valid(j.position_valid); joint->set_position_rad(j.position_valid ? j.position_rad : 0);
        joint->set_velocity_valid(j.velocity_valid); joint->set_velocity_radps(j.velocity_valid ? j.velocity_radps : 0);
        joint->set_effort_valid(j.effort_valid); joint->set_effort_nm(j.effort_valid ? j.effort_nm : 0);
        joint->set_temperature_valid(j.temperature_valid); joint->set_temperature_celsius(j.temperature_valid ? j.temperature_celsius : 0);
        joint->set_voltage_valid(j.voltage_valid); joint->set_voltage_v(j.voltage_valid ? j.voltage_v : 0);
        joint->set_fault_valid(j.fault_valid); joint->set_fault_code(j.fault_valid ? j.fault_code : 0);
    }
    return Result<wire::ControllerFeedback>::Success(std::move(out));
}

}  // namespace openember::services::hardware_interface::actuator_control
