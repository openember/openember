/*
 * Copyright (c) 2022-2026, OpenEmber Team
 * SPDX-License-Identifier: Apache-2.0
 *
 * Host-side turtlesim-style world: subscribe /turtle/cmd_vel, integrate Pose2D,
 * publish /turtle/pose. Default view is an SDL2 window on Linux; use --no-gui
 * for ASCII / headless runs.
 */

#include <chrono>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

#include "turtle_common.hpp"
#include "turtle_view_sdl.hpp"

#include "openember/init.hpp"
#include "openember/msgs/geometry/v1/geometry.pb.h"
#include "openember/node.hpp"

namespace {

constexpr const char* kNodeName = "turtle_sim";
constexpr double kDtSec = 0.05;
constexpr double kCommandTimeoutSec = 0.5;
constexpr double kWorldHalfExtent = 10.0;
constexpr int kMapWidth = 41;
constexpr int kMapHeight = 21;
constexpr int kWindowSizePx = 720;

struct CommandState {
    double linear_x = 0.0;
    double angular_z = 0.0;
    std::chrono::steady_clock::time_point last_rx =
        std::chrono::steady_clock::time_point::min();
};

struct SimCliOptions {
    turtle_demo::LinkCliOptions link;
    bool no_gui = false;
};

SimCliOptions ParseSimOptions(int argc, char** argv) {
    SimCliOptions options;
    options.link = turtle_demo::ParseLinkOptions(argc, argv);
    for (int i = 1; i < argc; ++i) {
        const std::string arg(argv[i]);
        if (arg == "--no-gui" || arg == "--headless") {
            options.no_gui = true;
        }
    }
    return options;
}

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

char HeadingGlyph(double theta) {
    constexpr double kPi = 3.14159265358979323846;
    const double sector = std::fmod(NormalizeAngle(theta) + 2.0 * kPi, 2.0 * kPi);
    const int idx = static_cast<int>(std::floor(sector / (kPi / 4.0) + 0.5)) % 8;
    static constexpr char kGlyphs[] = {'>', '/', '^', '\\', '<', '/', 'v', '\\'};
    return kGlyphs[idx];
}

void PrintAsciiMap(double x, double y, double theta) {
    const double span = 2.0 * kWorldHalfExtent;
    int col = static_cast<int>(std::lround((x + kWorldHalfExtent) / span * (kMapWidth - 1)));
    int row = static_cast<int>(std::lround((kWorldHalfExtent - y) / span * (kMapHeight - 1)));
    col = static_cast<int>(Clamp(col, 0, kMapWidth - 1));
    row = static_cast<int>(Clamp(row, 0, kMapHeight - 1));
    const char turtle = HeadingGlyph(theta);

    std::cout << "\n+";
    for (int c = 0; c < kMapWidth; ++c) {
        std::cout << '-';
    }
    std::cout << "+\n";
    for (int r = 0; r < kMapHeight; ++r) {
        std::cout << '|';
        for (int c = 0; c < kMapWidth; ++c) {
            if (r == row && c == col) {
                std::cout << turtle;
            } else if (r == kMapHeight / 2 && c == kMapWidth / 2) {
                std::cout << '+';
            } else {
                std::cout << ' ';
            }
        }
        std::cout << "|\n";
    }
    std::cout << '+';
    for (int c = 0; c < kMapWidth; ++c) {
        std::cout << '-';
    }
    std::cout << "+\n";
    std::cout << "pose x=" << x << " y=" << y << " theta=" << theta
              << " (" << turtle << ")\n"
              << std::flush;
}

}  // namespace

int main(int argc, char** argv) {
    try {
        const auto cli = ParseSimOptions(argc, argv);
        const auto link_mode = turtle_demo::InitLink(cli.link);

        auto node = openember::CreateNode(kNodeName);
        auto pose_pub =
            node->Advertise<openember::msgs::geometry::v1::Pose2D>(turtle_demo::kPoseTopic);

        std::mutex cmd_mutex;
        CommandState command;

        auto cmd_sub = node->Subscribe<openember::msgs::geometry::v1::Twist2D>(
            turtle_demo::kCmdVelTopic,
            [&](const openember::msgs::geometry::v1::Twist2D& twist) {
                std::lock_guard<std::mutex> lock(cmd_mutex);
                command.linear_x = twist.linear_x();
                command.angular_z = twist.angular_z();
                command.last_rx = std::chrono::steady_clock::now();
            });

        std::unique_ptr<TurtleViewSdl> view;
        if (!cli.no_gui) {
            view = std::make_unique<TurtleViewSdl>(kWorldHalfExtent, kWindowSizePx);
            if (!view->Ok()) {
                std::cerr << "SDL window unavailable; falling back to --no-gui\n";
                view.reset();
            }
        }

        double x = 0.0;
        double y = 0.0;
        double theta = 0.0;
        std::uint64_t pose_sequence = 0;
        auto last_ascii = std::chrono::steady_clock::now();

        std::cout << kNodeName << " started as Link " << link_mode
                  << (view ? " with SDL GUI" : " (ASCII/no-gui)")
                  << "\n  subscribe " << turtle_demo::kCmdVelTopic
                  << "\n  publish   " << turtle_demo::kPoseTopic
                  << "\n  close window / Esc to quit GUI mode\n"
                  << std::flush;

        while (openember::Ok()) {
            if (view && !view->Pump()) {
                openember::Shutdown();
                break;
            }

            const auto now = std::chrono::steady_clock::now();
            double linear_x = 0.0;
            double angular_z = 0.0;
            {
                std::lock_guard<std::mutex> lock(cmd_mutex);
                const auto age = std::chrono::duration<double>(now - command.last_rx).count();
                if (command.last_rx != std::chrono::steady_clock::time_point::min() &&
                    age <= kCommandTimeoutSec) {
                    linear_x = command.linear_x;
                    angular_z = command.angular_z;
                }
            }

            x += linear_x * std::cos(theta) * kDtSec;
            y += linear_x * std::sin(theta) * kDtSec;
            theta = NormalizeAngle(theta + angular_z * kDtSec);
            x = Clamp(x, -kWorldHalfExtent, kWorldHalfExtent);
            y = Clamp(y, -kWorldHalfExtent, kWorldHalfExtent);

            auto pose = turtle_demo::MakePose(kNodeName, pose_sequence, x, y, theta);
            if (!pose_pub.Publish(pose)) {
                std::cerr << kNodeName << " pose publish failed\n";
            }
            ++pose_sequence;

            if (view) {
                view->Draw(x, y, theta);
            } else if (now - last_ascii >= std::chrono::milliseconds(500)) {
                PrintAsciiMap(x, y, theta);
                last_ascii = now;
            }

            std::this_thread::sleep_for(
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::duration<double>(kDtSec)));
        }

        view.reset();
        openember::Shutdown();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "turtle_sim failed: " << e.what() << std::endl;
        return 1;
    }
}
