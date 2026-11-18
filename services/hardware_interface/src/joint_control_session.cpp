#include "openember/services/hardware_interface/joint_control_session.hpp"

#include <stdexcept>
#include <utility>

namespace openember::services::hardware_interface {
namespace {
actuator_control::domain::Backend& CheckedBackend(
    const std::unique_ptr<actuator_control::domain::Backend>& backend) {
    if (!backend) throw std::invalid_argument("joint control session requires a backend");
    return *backend;
}
}

JointControlSession::JointControlSession(actuator_control::domain::Capabilities capabilities,
    std::unique_ptr<actuator_control::domain::Backend> backend,
    actuator_control::domain::Guard::TokenFactory tokens)
    : backend_(std::move(backend)), guard_(std::move(capabilities), CheckedBackend(backend_), std::move(tokens)) {}

actuator_control::domain::Decision JointControlSession::Command(
    const actuator_control::wire::JointCommand& message, std::uint64_t now) {
    auto decoded = actuator_control::FromProto(message);
    if (!decoded.Ok()) return guard_.RejectInput(decoded.Err().code == hardware::ErrorCode::kNotSupported ?
        actuator_control::domain::Reason::kUnsupportedMode : actuator_control::domain::Reason::kInvalidArgument, now);
    return guard_.Accept(decoded.Value(), now);
}

actuator_control::Result<actuator_control::wire::ControlResult> JointControlSession::Control(
    const actuator_control::wire::ControlRequest& message, std::uint64_t now,
    actuator_control::domain::Permission permission) {
    guard_.Tick(now);
    auto decoded = actuator_control::FromProto(message);
    if (!decoded.Ok()) return actuator_control::Result<actuator_control::wire::ControlResult>::Failure(decoded.Err());
    const auto result = guard_.Control(decoded.Value(), now, permission);
    return actuator_control::ToProto(guard_.Info(), decoded.Value(), result);
}

actuator_control::Result<actuator_control::wire::ControllerCapabilities> JointControlSession::Capabilities() const {
    return actuator_control::ToProto(guard_.Info());
}

actuator_control::Result<actuator_control::wire::ControllerFeedback> JointControlSession::Feedback(
    const actuator_control::domain::FeedbackFrame& frame) const {
    return actuator_control::ToProto(guard_.Info(), guard_.Snapshot(), frame);
}

}  // namespace openember::services::hardware_interface
