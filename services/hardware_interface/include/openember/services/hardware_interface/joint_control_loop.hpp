#pragma once

#include <chrono>
#include <functional>
#include <future>
#include <memory>
#include <optional>
#include <string>

#include "openember/services/hardware_interface/joint_control_session.hpp"

namespace openember::services::hardware_interface {

struct JointControlLoopOptions {
    actuator_control::domain::Capabilities capabilities;
    std::chrono::nanoseconds period{1000000};
    // Both callbacks and Backend methods execute only on the worker. Reader may
    // advance a simulation once; it must not fabricate per-joint samples.
    std::function<std::unique_ptr<actuator_control::domain::Backend>()> backend;
    std::function<actuator_control::domain::FeedbackFrame()> read;
    actuator_control::domain::Guard::TokenFactory tokens;
    // Same-host CLOCK_MONOTONIC; thread-safe (ingress also checks stop deadlines).
    std::function<std::uint64_t()> clock;
};

// One-shot worker; restart/reset requires a new loop with a fresh session.
// This is an in-process API, not an authenticated network endpoint.
class JointControlLoop {
public:
    explicit JointControlLoop(JointControlLoopOptions options);
    ~JointControlLoop();
    JointControlLoop(const JointControlLoop&) = delete;
    JointControlLoop& operator=(const JointControlLoop&) = delete;
    bool Start();
    void RequestStop() noexcept; // Independent of queue capacity; no blocking join.
    void Stop() noexcept;        // External owner joins; do not call from callbacks.
    std::future<actuator_control::Result<actuator_control::wire::ControlResult>> Control(
        const actuator_control::wire::ControlRequest& request, actuator_control::domain::Permission permission);
    std::future<actuator_control::domain::Decision> Command(const actuator_control::wire::JointCommand& command);
    actuator_control::domain::Status Status() const;
    std::optional<actuator_control::wire::ControllerFeedback> Latest() const;
    std::string Error() const;
    bool Running() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace openember::services::hardware_interface
