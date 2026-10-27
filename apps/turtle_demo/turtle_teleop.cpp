/*
 * Copyright (c) 2022-2026, OpenEmber Team
 * SPDX-License-Identifier: Apache-2.0
 *
 * Keyboard / scripted teleop for turtle_sim.
 *   --demo   publish a continuous circle (headless / launch friendly)
 *   keys     w/s forward-back, a/d turn, space stop, q quit
 */

#include <chrono>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <string>
#include <thread>

#include <fcntl.h>
#include <termios.h>
#include <unistd.h>

#include "turtle_common.hpp"

#include "openember/init.hpp"
#include "openember/msgs/geometry/v1/geometry.pb.h"
#include "openember/node.hpp"

namespace {

constexpr const char* kNodeName = "turtle_teleop";
constexpr double kLinearSpeed = 1.0;
constexpr double kAngularSpeed = 1.0;

struct CliOptions {
    turtle_demo::LinkCliOptions link;
    bool demo = false;
};

CliOptions ParseOptions(int argc, char** argv) {
    CliOptions options;
    options.link = turtle_demo::ParseLinkOptions(argc, argv);
    for (int i = 1; i < argc; ++i) {
        const std::string arg(argv[i]);
        if (arg == "--demo") {
            options.demo = true;
        }
    }
    return options;
}

class RawTerminal {
public:
    RawTerminal() {
        if (!isatty(STDIN_FILENO)) {
            return;
        }
        if (tcgetattr(STDIN_FILENO, &original_) != 0) {
            return;
        }
        termios raw = original_;
        raw.c_lflag &= static_cast<tcflag_t>(~(ICANON | ECHO));
        raw.c_cc[VMIN] = 0;
        raw.c_cc[VTIME] = 0;
        if (tcsetattr(STDIN_FILENO, TCSANOW, &raw) != 0) {
            return;
        }
        const int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
        if (flags >= 0) {
            fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);
        }
        active_ = true;
    }

    ~RawTerminal() {
        if (active_) {
            tcsetattr(STDIN_FILENO, TCSANOW, &original_);
        }
    }

    bool Active() const {
        return active_;
    }

    int ReadKey() {
        if (!active_) {
            return -1;
        }
        unsigned char ch = 0;
        const ssize_t n = ::read(STDIN_FILENO, &ch, 1);
        if (n == 1) {
            return static_cast<int>(ch);
        }
        return -1;
    }

private:
    termios original_{};
    bool active_ = false;
};

}  // namespace

int main(int argc, char** argv) {
    try {
        const auto cli = ParseOptions(argc, argv);
        const auto link_mode = turtle_demo::InitLink(cli.link);

        auto node = openember::CreateNode(kNodeName);
        auto cmd_pub =
            node->Advertise<openember::msgs::geometry::v1::Twist2D>(turtle_demo::kCmdVelTopic);

        std::uint64_t sequence = 0;
        double linear_x = 0.0;
        double angular_z = 0.0;
        const auto start = std::chrono::steady_clock::now();

        std::cout << kNodeName << " started as Link " << link_mode
                  << " on " << turtle_demo::kCmdVelTopic
                  << (cli.demo ? " (--demo circle)" : " (keyboard)")
                  << "\n" << std::flush;

        if (!cli.demo) {
            std::cout << "keys: w/s forward-back, a/d turn, space stop, q quit\n"
                      << std::flush;
        }

        RawTerminal terminal;
        if (!cli.demo && !terminal.Active()) {
            std::cerr << "stdin is not a TTY; use --demo for headless teleop\n";
            openember::Shutdown();
            return 1;
        }

        while (openember::Ok()) {
            if (cli.demo) {
                const auto elapsed =
                    std::chrono::duration<double>(std::chrono::steady_clock::now() - start)
                        .count();
                linear_x = 0.8;
                angular_z = 0.6 + 0.2 * std::sin(elapsed);
            } else {
                const int key = terminal.ReadKey();
                if (key == 'q' || key == 'Q') {
                    break;
                }
                if (key == 'w' || key == 'W') {
                    linear_x = kLinearSpeed;
                } else if (key == 's' || key == 'S') {
                    linear_x = -kLinearSpeed;
                } else if (key == 'a' || key == 'A') {
                    angular_z = kAngularSpeed;
                } else if (key == 'd' || key == 'D') {
                    angular_z = -kAngularSpeed;
                } else if (key == ' ') {
                    linear_x = 0.0;
                    angular_z = 0.0;
                }
            }

            auto twist =
                turtle_demo::MakeTwist(kNodeName, sequence, linear_x, angular_z);
            if (!cmd_pub.Publish(twist)) {
                std::cerr << kNodeName << " cmd_vel publish failed\n";
            } else if (sequence % 20 == 0) {
                std::cout << "cmd_vel linear_x=" << linear_x
                          << " angular_z=" << angular_z << '\n'
                          << std::flush;
            }
            ++sequence;

            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }

        // Publish a final stop command.
        (void)cmd_pub.Publish(turtle_demo::MakeTwist(kNodeName, sequence, 0.0, 0.0));
        openember::Shutdown();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "turtle_teleop failed: " << e.what() << std::endl;
        return 1;
    }
}
