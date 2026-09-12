#pragma once

#include "adapters/message_header_context.hpp"
#include "openember/actuator/joint_types.hpp"
#include "openember/msgs/actuator/v1/actuator.pb.h"

namespace openember::services::hardware_interface {

openember::actuator::JointCommand ToDomain(
    const openember::msgs::actuator::v1::JointCommand& msg);

openember::msgs::actuator::v1::JointState ToProto(
    const openember::actuator::JointState& state,
    const MessageHeaderContext& context);

openember::msgs::actuator::v1::JointControllerStatus ToProto(
    const openember::actuator::ActuatorStatus& status,
    const std::string& controller_id,
    const MessageHeaderContext& context);

}  // namespace openember::services::hardware_interface
