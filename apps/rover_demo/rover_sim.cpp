/*
 * Copyright (c) 2022-2026, OpenEmber Team
 * SPDX-License-Identifier: Apache-2.0
 *
 * Host-side differential-drive rover world:
 *   subscribe /rover/cmd_vel and /rover/estop
 *   publish /rover/odom (Pose2D)
 * Command loss or e-stop forces a local stop (safety demo).
 */

#include <chrono>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>

#include "rover_common.hpp"

#include "openember/init.hpp"
#include "openember/msgs/common/v1/common.pb.h"
#include "openember/msgs/geometry/v1/geometry.pb.h"
#include "openember/node.hpp"

namespace {

constexpr const char* kNodeName = "rover_sim";
constexpr double kDtSec = 0.05;
constexpr double kCommandTimeoutSec = 0.5;
constexpr double kWorldHalfExtent = 20.0;

struct CommandState {
    double linear_x = 0.0;
    double angular_z = 0.0;
    std::chrono::steady_clock::time_point last_rx =
        std::chrono::steady_clock::time_point::min();
};

double NormalizeAngle(double theta) {
    constexpr double kPi = 3.14159265358979323846;
    while (theta > kPi) {
        theta -= 2.0 * kPi;
    }
    while (theta < -kPi) {
        theta += 2.0 * kPi;
    }
    return theta;
}

double Clamp(double value, double lo, double hi) {
    if (value < lo) {
        return lo;
    }
    if (value > hi) {
        return hi;
    }
    return value;
}

}  // namespace

int main(int argc, char** argv) {
    try {
        const auto link = rover_demo::ParseLinkOptions(argc, argv);
        const auto link_mode = rover_demo::InitLink(link);

        auto node = openember::CreateNode(kNodeName);
        auto odom_pub =
            node->Advertise<openember::msgs::geometry::v1::Pose2D>(rover_demo::kOdomTopic);

        std::mutex state_mutex;
        CommandState command;
        bool estop_engaged = false;

        auto cmd_sub = node->Subscribe<openember::msgs::geometry::v1::Twist2D>(
            rover_demo::kCmdVelTopic,
            [&](const openember::msgs::geometry::v1::Twist2D& twist) {
                std::lock_guard<std::mutex> lock(state_mutex);
                command.linear_x = twist.linear_x();
                command.angular_z = twist.angular_z();
                command.last_rx = std::chrono::steady_clock::now();
            });

        auto estop_sub = node->Subscribe<openember::msgs::common::v1::BoolStamped>(
            rover_demo::kEstopTopic,
            [&](const openember::msgs::common::v1::BoolStamped& msg) {
                std::lock_guard<std::mutex> lock(state_mutex);
                estop_engaged = msg.value();
            });

        double x = 0.0;
        double y = 0.0;
        double theta = 0.0;
        std::uint64_t odom_sequence = 0;
        auto last_log = std::chrono::steady_clock::now();

        std::cout << kNodeName << " started as Link " << link_mode
                  << "\n  subscribe " << rover_demo::kCmdVelTopic
                  << "\n  subscribe " << rover_demo::kEstopTopic
                  << "\n  publish   " << rover_demo::kOdomTopic
                  << "\n  safety: cmd timeout " << kCommandTimeoutSec
                  << "s or estop => stop\n"
                  << std::flush;

        while (openember::Ok()) {
            const auto now = std::chrono::steady_clock::now();
            double linear_x = 0.0;
            double angular_z = 0.0;
            bool stopped_for_safety = false;
            {
                std::lock_guard<std::mutex> lock(state_mutex);
                if (estop_engaged) {
                    stopped_for_safety = true;
                } else {
                    const auto age =
                        std::chrono::duration<double>(now - command.last_rx).count();
                    if (command.last_rx != std::chrono::steady_clock::time_point::min() &&
                        age <= kCommandTimeoutSec) {
                        linear_x = command.linear_x;
                        angular_z = command.angular_z;
                    } else if (command.last_rx !=
                               std::chrono::steady_clock::time_point::min()) {
                        stopped_for_safety = true;
                    }
                }
            }

            x += linear_x * std::cos(theta) * kDtSec;
            y += linear_x * std::sin(theta) * kDtSec;
            theta = NormalizeAngle(theta + angular_z * kDtSec);
            x = Clamp(x, -kWorldHalfExtent, kWorldHalfExtent);
            y = Clamp(y, -kWorldHalfExtent, kWorldHalfExtent);

            auto odom = rover_demo::MakeOdom(kNodeName, odom_sequence, x, y, theta);
            if (!odom_pub.Publish(odom)) {
                std::cerr << kNodeName << " odom publish failed\n";
            }
            ++odom_sequence;

            if (now - last_log >= std::chrono::milliseconds(500)) {
                std::cout << "odom x=" << x << " y=" << y << " theta=" << theta
                          << (stopped_for_safety ? " [SAFE-STOP]" : "") << '\n'
                          << std::flush;
                last_log = now;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(
                static_cast<int>(kDtSec * 1000.0)));
        }

        openember::Shutdown();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "rover_sim failed: " << e.what() << std::endl;
        return 1;
    }
}
