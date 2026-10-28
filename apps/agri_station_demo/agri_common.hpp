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
#include "openember/msgs/sensor/v1/sensor.pb.h"

namespace agri_demo {

constexpr const char* kRobotId = "openember";
constexpr const char* kInstanceId = "agri-station-demo";

constexpr const char* kAirTempTopic = "/agri/air/temperature";
constexpr const char* kAirHumidityTopic = "/agri/air/humidity";
constexpr const char* kLightTopic = "/agri/light/illuminance";
constexpr const char* kSoilMoistureTopic = "/agri/soil/moisture";
constexpr const char* kStationStatusTopic = "/agri/station/status";
constexpr const char* kStationAlertTopic = "/agri/station/alert";

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

inline std::uint64_t MonotonicNs() {
    const auto now = std::chrono::steady_clock::now().time_since_epoch();
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

inline openember::msgs::sensor::v1::TemperatureSample MakeTemperature(
    const std::string& node_name,
    const std::string& sensor_id,
    std::uint64_t sequence,
    double celsius) {
    openember::msgs::sensor::v1::TemperatureSample sample;
    FillHeader(sample.mutable_header(), node_name, sequence);
    sample.set_sensor_id(sensor_id);
    sample.set_frame_id("agri_station");
    sample.set_sample_time_monotonic_ns(MonotonicNs());
    sample.set_sequence(sequence);
    sample.set_temperature_celsius(celsius);
    return sample;
}

inline openember::msgs::sensor::v1::ScalarSample MakeScalar(
    const std::string& node_name,
    const std::string& sensor_id,
    std::uint64_t sequence,
    const std::string& quantity,
    double value,
    const std::string& unit) {
    openember::msgs::sensor::v1::ScalarSample sample;
    FillHeader(sample.mutable_header(), node_name, sequence);
    sample.set_sensor_id(sensor_id);
    sample.set_frame_id("agri_station");
    sample.set_sample_time_monotonic_ns(MonotonicNs());
    sample.set_sequence(sequence);
    sample.set_quantity(quantity);
    sample.set_value(value);
    sample.set_unit(unit);
    return sample;
}

}  // namespace agri_demo
