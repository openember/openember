#pragma once

#include <atomic>
#include <mutex>
#include <string>
#include <thread>

#include "openember/actuator/mock_joint_controller.hpp"
#include "openember/msgs/actuator/v1/actuator.pb.h"
#include "openember/publisher.hpp"
#include "openember/services/hardware_interface/endpoint.hpp"
#include "openember/subscriber.hpp"

namespace openember::services::hardware_interface {

class JointControllerEndpoint final : public HardwareEndpoint {
public:
    JointControllerEndpoint(EndpointConfig config, EndpointContext context);
    ~JointControllerEndpoint() override;

    const std::string& Id() const override;
    bool Configure() override;
    bool Start() override;
    void Stop() noexcept override;
    EndpointStatus Status() const override;

private:
    void Run();
    void HandleCommand(const openember::msgs::actuator::v1::JointCommand& msg);
    void RecordError(const std::string& message);
    void RecordTimeout(const std::string& message);
    void SetError(const std::string& message);
    void UpdateStatusFromController(
        const openember::actuator::JointState& state,
        bool published);

    EndpointConfig config_;
    EndpointContext context_;
    openember::actuator::MockJointController controller_;
    openember::Publisher<openember::msgs::actuator::v1::JointState> state_publisher_;
    openember::Subscriber<openember::msgs::actuator::v1::JointCommand> command_subscriber_;
    std::atomic_bool running_{false};
    std::thread thread_;
    mutable std::mutex status_mutex_;
    EndpointStatus status_;
};

}  // namespace openember::services::hardware_interface
