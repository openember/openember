#include <chrono>
#include <cstdint>
#include <iostream>
#include <string>
#include <thread>

#include "openember/init.hpp"
#include "openember/link/options.hpp"
#include "openember/msgs/actuator/v1/actuator.pb.h"
#include "openember/node.hpp"

namespace {

constexpr const char* kRobotId = "openember";
constexpr const char* kNodeName = "joint_command_sender";

std::uint64_t UnixTimeNs() {
    const auto now = std::chrono::system_clock::now().time_since_epoch();
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(now).count());
}

std::uint64_t SteadyTimeNs() {
    const auto now = std::chrono::steady_clock::now().time_since_epoch();
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(now).count());
}

void FillHeader(openember::msgs::common::v1::Header* header,
                std::uint64_t sequence) {
    header->set_source_node(kNodeName);
    header->set_source_instance(kNodeName);
    header->set_robot_id(kRobotId);
    header->set_sequence(sequence);
    header->set_timestamp_unix_ns(UnixTimeNs());
}

struct Options {
    std::string topic = "/actuators/joints/joint_controller0/command";
    std::string controller_id = "joint_controller0";
    double position_rad = 0.5;
    int count = 20;
    int period_ms = 50;
    bool disable_at_end = true;
};

Options ParseOptions(int argc, char** argv) {
    Options options;
    for (int i = 1; i < argc; ++i) {
        const std::string arg(argv[i]);
        if (arg == "--topic" && i + 1 < argc) {
            options.topic = argv[++i];
        } else if (arg == "--controller" && i + 1 < argc) {
            options.controller_id = argv[++i];
        } else if (arg == "--position" && i + 1 < argc) {
            options.position_rad = std::stod(argv[++i]);
        } else if (arg == "--count" && i + 1 < argc) {
            options.count = std::stoi(argv[++i]);
        } else if (arg == "--period-ms" && i + 1 < argc) {
            options.period_ms = std::stoi(argv[++i]);
        } else if (arg == "--no-disable") {
            options.disable_at_end = false;
        }
    }
    return options;
}

}  // namespace

int main(int argc, char** argv) {
    const auto cli = ParseOptions(argc, argv);

    openember::RuntimeOptions options;
    options.robot_id = kRobotId;
    options.link = openember::link::LocalClientOptions();
    openember::Init(options);

    auto node = openember::CreateNode(kNodeName);
    auto pub =
        node->Advertise<openember::msgs::actuator::v1::JointCommand>(
            cli.topic);

    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    for (int i = 0; openember::Ok() && i < cli.count; ++i) {
        openember::msgs::actuator::v1::JointCommand command;
        FillHeader(command.mutable_header(), static_cast<std::uint64_t>(i));
        command.set_controller_id(cli.controller_id);
        command.set_command_time_monotonic_ns(SteadyTimeNs());
        command.set_sequence(static_cast<std::uint64_t>(i));
        command.set_enable(true);

        auto* joint = command.add_joints();
        joint->set_joint_id(1);
        joint->set_joint_name("joint_1");
        joint->set_mode(openember::msgs::actuator::v1::JOINT_CONTROL_MODE_POSITION);
        joint->set_position_enabled(true);
        joint->set_position_rad(cli.position_rad);
        joint->set_effort_enabled(true);
        joint->set_effort_nm(1.0);
        joint->set_max_effort_nm(5.0);

        if (pub.Publish(command)) {
            std::cout << node->Name()
                      << " publish JointCommand sequence=" << i
                      << " position_rad=" << cli.position_rad
                      << std::endl;
        } else {
            std::cerr << "publish JointCommand failed" << std::endl;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(cli.period_ms));
    }

    if (cli.disable_at_end) {
        openember::msgs::actuator::v1::JointCommand disable;
        FillHeader(disable.mutable_header(), static_cast<std::uint64_t>(cli.count));
        disable.set_controller_id(cli.controller_id);
        disable.set_command_time_monotonic_ns(SteadyTimeNs());
        disable.set_sequence(static_cast<std::uint64_t>(cli.count));
        disable.set_disable(true);
        (void)pub.Publish(disable);
    }

    openember::Shutdown();
    return 0;
}
