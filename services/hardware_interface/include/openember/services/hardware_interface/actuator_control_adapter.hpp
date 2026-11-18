#pragma once

#include "openember/actuator/control_feedback.hpp"
#include "openember/actuator/control_guard.hpp"
#include "openember/hardware/result.hpp"
#include "openember/msgs/actuator/v2/actuator.pb.h"

namespace openember::services::hardware_interface::actuator_control {

namespace domain = openember::actuator::control;
namespace wire = openember::msgs::actuator::v2;
template <typename T> using Result = openember::hardware::Result<T>;

// No implicit timestamps, missing-field defaults, enum casts or truncation.
// Context identity/time/authority and topology limits remain Guard's job.
Result<domain::Command> FromProto(const wire::JointCommand& message);
Result<domain::Request> FromProto(const wire::ControlRequest& message);
Result<domain::Capabilities> FromProto(const wire::ControllerCapabilities& message);
Result<wire::ControllerCapabilities> ToProto(const domain::Capabilities& capabilities);
Result<wire::ControlResult> ToProto(const domain::Capabilities& capabilities,
                                  const domain::Request& request,
                                  const domain::ControlResult& result);
Result<wire::ControllerFeedback> ToProto(const domain::Capabilities& capabilities,
                                       const domain::Status& status,
                                       const domain::FeedbackFrame& frame);

}  // namespace openember::services::hardware_interface::actuator_control
