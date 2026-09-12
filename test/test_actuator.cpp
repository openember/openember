#include "minunit.h"

#include <chrono>
#include <thread>

#include "openember/actuator/mock_joint_controller.hpp"
#include "openember/hardware/timestamp.hpp"

namespace actuator = openember::actuator;

MU_TEST(test_command_timeout_activates_safe_output)
{
    actuator::MockJointControllerOptions options;
    options.state_rate_hz = 1000.0;
    options.command_timeout_ms = 5;
    options.disabled_on_start = false;

    actuator::MockJointController controller(options);
    mu_check(controller.Start().Ok());

    actuator::JointCommand command;
    command.controller_id = options.controller_id;
    command.command_time_monotonic_ns = openember::hardware::SteadyTimeNs();
    actuator::JointCommandItem joint;
    joint.joint_id = 1;
    joint.mode = actuator::JointControlMode::kEffort;
    joint.effort_enabled = true;
    joint.effort_nm = 2.0;
    command.joints.push_back(joint);

    mu_check(controller.SetCommand(command).Ok());
    mu_check(controller.Step().Ok());
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    mu_check(controller.Step().Ok());

    const auto status = controller.Status();
    mu_check(status.timeout_count == 1);
    mu_check(status.availability == actuator::ActuatorAvailability::kDisabled);
    mu_check(status.last_error.code == openember::hardware::ErrorCode::kTimeout);

    auto state = controller.ReadState(std::chrono::milliseconds(10));
    mu_check(state.Ok());
    mu_check(!state.Value().joints.empty());
    mu_check(state.Value().joints.front().effort_nm == 0.0);

    controller.Stop();
}

MU_TEST_SUITE(test_suite)
{
    MU_RUN_TEST(test_command_timeout_activates_safe_output);
}

int main()
{
    MU_RUN_SUITE(test_suite);
    MU_REPORT();
    return MU_EXIT_CODE;
}
