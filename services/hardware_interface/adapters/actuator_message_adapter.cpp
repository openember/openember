#include "adapters/actuator_message_adapter.hpp"

#include <cstddef>
#include <utility>

#include "openember/hardware/timestamp.hpp"

namespace openember::services::hardware_interface {

namespace {

void FillHeader(openember::msgs::common::v1::Header* header,
                const MessageHeaderContext& context,
                std::uint64_t sequence) {
    header->set_source_node(context.source_node);
    header->set_source_instance(context.source_instance);
    header->set_robot_id(context.robot_id);
    header->set_sequence(sequence);
    header->set_timestamp_unix_ns(context.timestamp_unix_ns);
}

openember::actuator::JointControlMode ToDomainMode(
    openember::msgs::actuator::v1::JointControlMode mode) {
    using ProtoMode = openember::msgs::actuator::v1::JointControlMode;
    switch (mode) {
    case ProtoMode::JOINT_CONTROL_MODE_DISABLED:
        return openember::actuator::JointControlMode::kDisabled;
    case ProtoMode::JOINT_CONTROL_MODE_POSITION:
        return openember::actuator::JointControlMode::kPosition;
    case ProtoMode::JOINT_CONTROL_MODE_VELOCITY:
        return openember::actuator::JointControlMode::kVelocity;
    case ProtoMode::JOINT_CONTROL_MODE_EFFORT:
        return openember::actuator::JointControlMode::kEffort;
    case ProtoMode::JOINT_CONTROL_MODE_MIT:
        return openember::actuator::JointControlMode::kMit;
    case ProtoMode::JOINT_CONTROL_MODE_UNSPECIFIED:
        return openember::actuator::JointControlMode::kUnspecified;
    }
    return openember::actuator::JointControlMode::kUnspecified;
}

openember::msgs::actuator::v1::JointControlMode ToProtoMode(
    openember::actuator::JointControlMode mode) {
    using ProtoMode = openember::msgs::actuator::v1::JointControlMode;
    switch (mode) {
    case openember::actuator::JointControlMode::kDisabled:
        return ProtoMode::JOINT_CONTROL_MODE_DISABLED;
    case openember::actuator::JointControlMode::kPosition:
        return ProtoMode::JOINT_CONTROL_MODE_POSITION;
    case openember::actuator::JointControlMode::kVelocity:
        return ProtoMode::JOINT_CONTROL_MODE_VELOCITY;
    case openember::actuator::JointControlMode::kEffort:
        return ProtoMode::JOINT_CONTROL_MODE_EFFORT;
    case openember::actuator::JointControlMode::kMit:
        return ProtoMode::JOINT_CONTROL_MODE_MIT;
    case openember::actuator::JointControlMode::kUnspecified:
        return ProtoMode::JOINT_CONTROL_MODE_UNSPECIFIED;
    }
    return ProtoMode::JOINT_CONTROL_MODE_UNSPECIFIED;
}

openember::msgs::actuator::v1::ActuatorAvailability ToProtoAvailability(
    openember::actuator::ActuatorAvailability availability) {
    using ProtoAvailability = openember::msgs::actuator::v1::ActuatorAvailability;
    switch (availability) {
    case openember::actuator::ActuatorAvailability::kOffline:
        return ProtoAvailability::ACTUATOR_AVAILABILITY_OFFLINE;
    case openember::actuator::ActuatorAvailability::kDisabled:
        return ProtoAvailability::ACTUATOR_AVAILABILITY_DISABLED;
    case openember::actuator::ActuatorAvailability::kEnabled:
        return ProtoAvailability::ACTUATOR_AVAILABILITY_ENABLED;
    case openember::actuator::ActuatorAvailability::kDegraded:
        return ProtoAvailability::ACTUATOR_AVAILABILITY_DEGRADED;
    case openember::actuator::ActuatorAvailability::kFault:
        return ProtoAvailability::ACTUATOR_AVAILABILITY_FAULT;
    case openember::actuator::ActuatorAvailability::kEmergencyStop:
        return ProtoAvailability::ACTUATOR_AVAILABILITY_EMERGENCY_STOP;
    case openember::actuator::ActuatorAvailability::kUnspecified:
        return ProtoAvailability::ACTUATOR_AVAILABILITY_UNSPECIFIED;
    }
    return ProtoAvailability::ACTUATOR_AVAILABILITY_UNSPECIFIED;
}

openember::msgs::common::v1::HealthState ToProtoHealth(
    openember::hardware::HealthState health) {
    using ProtoHealth = openember::msgs::common::v1::HealthState;
    switch (health) {
    case openember::hardware::HealthState::kOk:
        return ProtoHealth::HEALTH_STATE_OK;
    case openember::hardware::HealthState::kWarning:
        return ProtoHealth::HEALTH_STATE_WARNING;
    case openember::hardware::HealthState::kError:
        return ProtoHealth::HEALTH_STATE_ERROR;
    case openember::hardware::HealthState::kStale:
        return ProtoHealth::HEALTH_STATE_STALE;
    case openember::hardware::HealthState::kUnspecified:
        return ProtoHealth::HEALTH_STATE_UNSPECIFIED;
    }
    return ProtoHealth::HEALTH_STATE_UNSPECIFIED;
}

}  // namespace

openember::actuator::JointCommand ToDomain(
    const openember::msgs::actuator::v1::JointCommand& msg) {
    openember::actuator::JointCommand command;
    command.controller_id = msg.controller_id();
    command.command_time_monotonic_ns = msg.command_time_monotonic_ns();
    command.sequence = msg.sequence();
    command.enable = msg.enable();
    command.disable = msg.disable();
    command.emergency_stop = msg.emergency_stop();
    command.clear_fault = msg.clear_fault();

    command.joints.reserve(static_cast<std::size_t>(msg.joints_size()));
    for (const auto& proto_item : msg.joints()) {
        openember::actuator::JointCommandItem item;
        item.joint_id = proto_item.joint_id();
        item.joint_name = proto_item.joint_name();
        item.mode = ToDomainMode(proto_item.mode());
        item.position_enabled = proto_item.position_enabled();
        item.position_rad = proto_item.position_rad();
        item.velocity_enabled = proto_item.velocity_enabled();
        item.velocity_radps = proto_item.velocity_radps();
        item.effort_enabled = proto_item.effort_enabled();
        item.effort_nm = proto_item.effort_nm();
        item.stiffness_enabled = proto_item.stiffness_enabled();
        item.stiffness_nm_per_rad = proto_item.stiffness_nm_per_rad();
        item.damping_enabled = proto_item.damping_enabled();
        item.damping_nms_per_rad = proto_item.damping_nms_per_rad();
        item.max_effort_nm = proto_item.max_effort_nm();
        command.joints.push_back(std::move(item));
    }

    return command;
}

openember::msgs::actuator::v1::JointState ToProto(
    const openember::actuator::JointState& state,
    const MessageHeaderContext& context) {
    openember::msgs::actuator::v1::JointState msg;
    FillHeader(msg.mutable_header(), context, state.sequence);
    msg.set_controller_id(state.controller_id);
    msg.set_sample_time_monotonic_ns(state.sample_time_monotonic_ns);
    msg.set_sequence(state.sequence);
    msg.set_availability(ToProtoAvailability(state.availability));
    msg.set_message(state.message);

    for (const auto& item : state.joints) {
        auto* proto_item = msg.add_joints();
        proto_item->set_joint_id(item.joint_id);
        proto_item->set_joint_name(item.joint_name);
        proto_item->set_mode(ToProtoMode(item.mode));
        proto_item->set_position_rad(item.position_rad);
        proto_item->set_velocity_radps(item.velocity_radps);
        proto_item->set_effort_nm(item.effort_nm);
        proto_item->set_temperature_celsius(item.temperature_celsius);
        proto_item->set_feedback_received(item.feedback_received);
        proto_item->set_fault_code(item.fault_code);
        proto_item->set_fault_message(item.fault_message);
    }

    return msg;
}

openember::msgs::actuator::v1::JointControllerStatus ToProto(
    const openember::actuator::ActuatorStatus& status,
    const std::string& controller_id,
    const MessageHeaderContext& context) {
    openember::msgs::actuator::v1::JointControllerStatus msg;
    FillHeader(msg.mutable_header(), context, status.sequence);
    msg.set_controller_id(controller_id);
    msg.set_availability(ToProtoAvailability(status.availability));
    msg.set_health(ToProtoHealth(status.health));
    msg.set_message(status.message);
    msg.set_last_command_age_ms(0);
    if (status.last_command_monotonic_ns != 0) {
        const auto now_ns = openember::hardware::SteadyTimeNs();
        if (now_ns > status.last_command_monotonic_ns) {
            msg.set_last_command_age_ms(
                (now_ns - status.last_command_monotonic_ns) / 1000000ULL);
        }
    }
    msg.set_command_timeout_ms(status.command_timeout_ms);

    auto* error_count = msg.add_metrics();
    error_count->set_name("error_count");
    error_count->set_value(static_cast<double>(status.error_count));
    error_count->set_unit("count");

    return msg;
}

}  // namespace openember::services::hardware_interface
