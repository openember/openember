#include <iostream>

#include "openember/init.hpp"
#include "openember/link/options.hpp"
#include "openember/msgs/actuator/v1/actuator.pb.h"
#include "openember/msgs/diagnostics/v1/diagnostics.pb.h"
#include "openember/msgs/sensor/v1/sensor.pb.h"
#include "openember/node.hpp"

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    openember::RuntimeOptions options;
    options.robot_id = "openember";
    options.link = openember::link::LocalRouterOptions();

    openember::Init(options);
    auto node = openember::CreateNode("hardware_mock_listener");

    auto imu_sub = node->Subscribe<openember::msgs::sensor::v1::ImuSample>(
        "/sensors/imu/imu0/sample",
        [](const openember::msgs::sensor::v1::ImuSample& sample) {
            std::cout << "imu"
                      << " seq=" << sample.sequence()
                      << " az=" << sample.acceleration_mps2().z()
                      << " temp_c=" << sample.temperature_celsius()
                      << std::endl;
        });

    auto temp_sub = node->Subscribe<openember::msgs::sensor::v1::TemperatureSample>(
        "/sensors/temperature/temp0/sample",
        [](const openember::msgs::sensor::v1::TemperatureSample& sample) {
            std::cout << "temperature"
                      << " seq=" << sample.sequence()
                      << " temp_c=" << sample.temperature_celsius()
                      << std::endl;
        });

    auto gnss_sub = node->Subscribe<openember::msgs::sensor::v1::GnssFix>(
        "/sensors/gnss/gnss0/fix",
        [](const openember::msgs::sensor::v1::GnssFix& fix) {
            std::cout << "gnss"
                      << " seq=" << fix.sequence()
                      << " lat=" << fix.latitude_deg()
                      << " lon=" << fix.longitude_deg()
                      << " sats=" << fix.satellites_used()
                      << std::endl;
        });

    auto diagnostics_sub =
        node->Subscribe<openember::msgs::diagnostics::v1::DiagnosticArray>(
            "/diagnostics/hardware_interface",
            [](const openember::msgs::diagnostics::v1::DiagnosticArray& array) {
                for (const auto& status : array.status()) {
                    if (status.name().find("hardware_interface.") != 0) {
                        continue;
                    }
                    std::cout << "diagnostic"
                              << " name=" << status.name()
                              << " hardware_id=" << status.hardware_id()
                              << " level=" << status.level()
                              << " message=\"" << status.message() << "\""
                              << std::endl;
                }
            });

    auto joint_state_sub =
        node->Subscribe<openember::msgs::actuator::v1::JointState>(
            "/actuators/joints/joint_controller0/state",
            [](const openember::msgs::actuator::v1::JointState& state) {
                std::cout << "joint_state"
                          << " seq=" << state.sequence()
                          << " availability=" << state.availability()
                          << " joints=" << state.joints_size()
                          << " message=\"" << state.message() << "\"";
                if (state.joints_size() > 0) {
                    const auto& joint = state.joints(0);
                    std::cout << " first_joint=" << joint.joint_name()
                              << " pos=" << joint.position_rad()
                              << " effort=" << joint.effort_nm();
                }
                std::cout << std::endl;
            });

    std::cout << "listening for Hardware Interface mock endpoint messages..."
              << std::endl;
    std::cout << "start openember_hardware_interface in another shell, press Enter to exit"
              << std::endl;
    std::cin.get();

    openember::Shutdown();
    return 0;
}
