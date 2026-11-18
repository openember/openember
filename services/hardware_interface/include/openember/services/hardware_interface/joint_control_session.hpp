#pragma once

#include <memory>

#include "openember/services/hardware_interface/actuator_control_adapter.hpp"

namespace openember::services::hardware_interface {

// Owner-thread HI composition, not a network endpoint. Backend ownership moves
// here and outlives Guard. No callbacks/threads/authentication are created.
class JointControlSession {
public:
    JointControlSession(actuator_control::domain::Capabilities capabilities,
                        std::unique_ptr<actuator_control::domain::Backend> backend,
                        actuator_control::domain::Guard::TokenFactory tokens);
    actuator_control::domain::Decision Command(
        const actuator_control::wire::JointCommand& message, std::uint64_t now_ns);
    actuator_control::Result<actuator_control::wire::ControlResult> Control(
        const actuator_control::wire::ControlRequest& message, std::uint64_t now_ns,
        actuator_control::domain::Permission permission);
    actuator_control::Result<actuator_control::wire::ControllerCapabilities> Capabilities() const;
    actuator_control::Result<actuator_control::wire::ControllerFeedback> Feedback(
        const actuator_control::domain::FeedbackFrame& frame) const;
    actuator_control::domain::Status Status() const { return guard_.Snapshot(); }
    void Tick(std::uint64_t now_ns) { guard_.Tick(now_ns); }
    void TripFault(actuator_control::domain::Reason reason, std::uint64_t now_ns) { guard_.TripFault(reason, now_ns); }
    actuator_control::domain::Decision Apply(std::uint64_t now_ns) { return guard_.Apply(now_ns); }
    actuator_control::domain::Decision SetPaused(bool paused, std::uint64_t now_ns) {
        return guard_.SetPaused(paused, now_ns);
    }
    bool Reset(std::string session) { return guard_.Reset(std::move(session)); }
    void Stop() noexcept { guard_.Stop(); }

private:
    std::unique_ptr<actuator_control::domain::Backend> backend_;
    actuator_control::domain::Guard guard_;
};

}  // namespace openember::services::hardware_interface
