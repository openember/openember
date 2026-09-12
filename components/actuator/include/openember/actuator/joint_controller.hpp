#pragma once

#include <chrono>
#include <vector>

#include "openember/actuator/actuator.hpp"
#include "openember/actuator/joint_types.hpp"

namespace openember::actuator {

class IJointController : public IActuator {
public:
    ~IJointController() override = default;

    virtual const std::vector<JointInfo>& Joints() const = 0;
    virtual hardware::Result<void> SetCommand(const JointCommand& command) = 0;
    virtual hardware::Result<void> Step() = 0;
    virtual hardware::Result<JointState> ReadState(
        std::chrono::milliseconds timeout) = 0;
};

}  // namespace openember::actuator
