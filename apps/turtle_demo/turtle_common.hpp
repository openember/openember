/*
 * Copyright (c) 2022-2026, OpenEmber Team
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <chrono>
#include <cstdint>
#include <exception>
#include <string>

#include "openember/init.hpp"
#include "openember/link/options.hpp"
#include "openember/msgs/common/v1/common.pb.h"
#include "openember/msgs/geometry/v1/geometry.pb.h"

namespace turtle_demo {

constexpr const char* kRobotId = "openember";
constexpr const char* kInstanceId = "turtle-demo";
constexpr const char* kCmdVelTopic = "/turtle/cmd_vel";
constexpr const char* kPoseTopic = "/turtle/pose";
constexpr const char* kWorldFrame = "world";

enum class LinkMode {
    kAuto,
    kRouter,
    kClient,
};

struct LinkCliOptions {
    LinkMode mode = LinkMode::kAuto;
    std::string connect = "tcp/127.0.0.1:7447";
    std::string listen = "tcp/127.0.0.1:7447";
};

inline std::uint64_t UnixTimeNs() {
    const auto now = std::chrono::system_clock::now().time_since_epoch();
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(now).count());
}

inline void FillHeader(openember::msgs::common::v1::Header* header,
                       const std::string& node_name,
                       std::uint64_t sequence) {
    header->set_source_node(node_name);
    header->set_source_instance(kInstanceId);
    header->set_robot_id(kRobotId);
    header->set_sequence(sequence);
    header->set_timestamp_unix_ns(UnixTimeNs());
}

inline void InitRouter(const LinkCliOptions& cli) {
    openember::RuntimeOptions options;
    options.robot_id = kRobotId;
    options.link.profile = openember::link::Profile::kRouter;
    options.link.listen = cli.listen;
    openember::Init(options);
}

inline void InitClient(const LinkCliOptions& cli) {
    openember::RuntimeOptions options;
    options.robot_id = kRobotId;
    options.link.profile = openember::link::Profile::kClient;
    options.link.connect = cli.connect;
    openember::Init(options);
}

inline std::string InitLink(const LinkCliOptions& cli) {
    if (cli.mode == LinkMode::kRouter) {
        InitRouter(cli);
        return "router";
    }

    if (cli.mode == LinkMode::kClient) {
        InitClient(cli);
        return "client";
    }

    try {
        InitRouter(cli);
        return "router";
    } catch (const std::exception&) {
        openember::Shutdown();
        InitClient(cli);
        return "client";
    }
}

inline LinkCliOptions ParseLinkOptions(int argc, char** argv) {
    LinkCliOptions options;
    for (int i = 1; i < argc; ++i) {
        const std::string arg(argv[i]);
        if (arg == "--router") {
            options.mode = LinkMode::kRouter;
        } else if (arg == "--client") {
            options.mode = LinkMode::kClient;
        } else if (arg == "--connect" && i + 1 < argc) {
            options.connect = argv[++i];
        } else if (arg == "--listen" && i + 1 < argc) {
            options.listen = argv[++i];
        }
    }
    return options;
}

inline openember::msgs::geometry::v1::Twist2D MakeTwist(
    const std::string& node_name,
    std::uint64_t sequence,
    double linear_x,
    double angular_z) {
    openember::msgs::geometry::v1::Twist2D twist;
    FillHeader(twist.mutable_header(), node_name, sequence);
    twist.set_frame_id(kWorldFrame);
    twist.set_linear_x(linear_x);
    twist.set_angular_z(angular_z);
    return twist;
}

inline openember::msgs::geometry::v1::Pose2D MakePose(
    const std::string& node_name,
    std::uint64_t sequence,
    double x,
    double y,
    double theta) {
    openember::msgs::geometry::v1::Pose2D pose;
    FillHeader(pose.mutable_header(), node_name, sequence);
    pose.set_frame_id(kWorldFrame);
    pose.set_x(x);
    pose.set_y(y);
    pose.set_theta(theta);
    return pose;
}

}  // namespace turtle_demo
