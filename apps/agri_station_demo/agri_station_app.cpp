/*
 * Copyright (c) 2022-2026, OpenEmber Team
 * SPDX-License-Identifier: Apache-2.0
 *
 * Product app for the agri station: subscribe mock sensors, evaluate
 * thresholds, publish station status / alerts and NodeHeartbeat.
 * Optional Qt Widgets field console (system Qt; --no-gui for headless).
 */

#include <chrono>
#include <cstdint>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>

#include "agri_common.hpp"
#include "agri_station_model.hpp"

#include "openember/init.hpp"
#include "openember/msgs/lifecycle/v1/lifecycle.pb.h"
#include "openember/msgs/node/v1/node.pb.h"
#include "openember/msgs/sensor/v1/sensor.pb.h"
#include "openember/node.hpp"

#if defined(OPENEMBER_AGRI_STATION_GUI) && OPENEMBER_AGRI_STATION_GUI
#include "agri_station_view_qt.hpp"

#include <QApplication>
#include <QTimer>
#endif

namespace {

constexpr const char* kNodeName = "agri_station_app";
constexpr const char* kHeartbeatTopic = "/nodes/agri_station_app/heartbeat";

struct CliOptions {
    agri_demo::LinkCliOptions link;
    bool no_gui = false;
};

CliOptions ParseOptions(int argc, char** argv) {
    CliOptions options;
    options.link = agri_demo::ParseLinkOptions(argc, argv);
    for (int i = 1; i < argc; ++i) {
        const std::string arg(argv[i]);
        if (arg == "--no-gui" || arg == "--headless") {
            options.no_gui = true;
        }
    }
    return options;
}

struct StationRuntime {
    std::mutex mu;
    agri_demo::SensorReading air_temp;
    agri_demo::SensorReading air_humidity;
    agri_demo::SensorReading light;
    agri_demo::SensorReading soil;
    std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
    std::uint64_t sequence = 0;

    openember::Publisher<openember::msgs::node::v1::NodeHeartbeat> heartbeat_pub;
    openember::Publisher<openember::msgs::sensor::v1::ScalarSample> status_pub;
    openember::Publisher<openember::msgs::sensor::v1::ScalarSample> alert_pub;
};

agri_demo::StationSnapshot TickOnce(StationRuntime& rt, bool log) {
    const auto now = std::chrono::steady_clock::now();
    agri_demo::SensorReading temp_snap;
    agri_demo::SensorReading humidity_snap;
    agri_demo::SensorReading light_snap;
    agri_demo::SensorReading soil_snap;
    {
        std::lock_guard<std::mutex> lock(rt.mu);
        temp_snap = rt.air_temp;
        humidity_snap = rt.air_humidity;
        light_snap = rt.light;
        soil_snap = rt.soil;
    }

    const auto snap = agri_demo::EvaluateStation(
        temp_snap, humidity_snap, light_snap, soil_snap, now);

    (void)rt.status_pub.Publish(agri_demo::MakeScalar(
        kNodeName,
        "station",
        rt.sequence,
        "station_state",
        snap.state_code,
        "enum"));

    if (snap.soil_dry) {
        (void)rt.alert_pub.Publish(agri_demo::MakeScalar(
            kNodeName,
            "station",
            rt.sequence,
            "soil_too_dry",
            snap.soil.value,
            "%"));
    }

    openember::msgs::node::v1::NodeHeartbeat heartbeat;
    agri_demo::FillHeader(heartbeat.mutable_header(), kNodeName, rt.sequence);
    heartbeat.set_node_name(kNodeName);
    heartbeat.set_instance_id(agri_demo::kInstanceId);
    heartbeat.set_lifecycle_state(
        openember::msgs::lifecycle::v1::LIFECYCLE_STATE_ACTIVE);
    heartbeat.set_health(
        snap.state == "fault"
            ? openember::msgs::common::v1::HEALTH_STATE_ERROR
            : (snap.state == "degraded" || snap.state == "alert"
                   ? openember::msgs::common::v1::HEALTH_STATE_WARNING
                   : openember::msgs::common::v1::HEALTH_STATE_OK));
    heartbeat.set_uptime_ms(static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(now - rt.start).count()));
    heartbeat.set_error_count(0);
    heartbeat.set_status_message("station_state=" + snap.state);

    auto* state_metric = heartbeat.add_metrics();
    state_metric->set_name("agri.station_state");
    state_metric->set_value(snap.state_code);
    state_metric->set_unit("enum");
    if (snap.soil_ok) {
        auto* soil_metric = heartbeat.add_metrics();
        soil_metric->set_name("agri.soil_moisture");
        soil_metric->set_value(snap.soil.value);
        soil_metric->set_unit("%");
    }
    if (agri_demo::Fresh(snap.temperature, now)) {
        auto* temp_metric = heartbeat.add_metrics();
        temp_metric->set_name("agri.air_temperature");
        temp_metric->set_value(snap.temperature.value);
        temp_metric->set_unit("C");
    }
    (void)rt.heartbeat_pub.Publish(heartbeat);

    if (log && rt.sequence % 2 == 0) {
        std::cout << kNodeName << " state=" << snap.state
                  << " soil="
                  << (snap.soil_ok ? std::to_string(snap.soil.value) : "stale")
                  << " temp="
                  << (agri_demo::Fresh(snap.temperature, now)
                          ? std::to_string(snap.temperature.value)
                          : "stale")
                  << '\n'
                  << std::flush;
    }

    ++rt.sequence;
    return snap;
}

}  // namespace

int main(int argc, char** argv) {
    try {
        const auto cli = ParseOptions(argc, argv);
        const auto link_mode = agri_demo::InitLink(cli.link);

        auto node = openember::CreateNode(kNodeName);
        StationRuntime rt;
        rt.heartbeat_pub =
            node->Advertise<openember::msgs::node::v1::NodeHeartbeat>(kHeartbeatTopic);
        rt.status_pub = node->Advertise<openember::msgs::sensor::v1::ScalarSample>(
            agri_demo::kStationStatusTopic);
        rt.alert_pub = node->Advertise<openember::msgs::sensor::v1::ScalarSample>(
            agri_demo::kStationAlertTopic);

        auto temp_sub = node->Subscribe<openember::msgs::sensor::v1::TemperatureSample>(
            agri_demo::kAirTempTopic,
            [&](const openember::msgs::sensor::v1::TemperatureSample& sample) {
                std::lock_guard<std::mutex> lock(rt.mu);
                rt.air_temp.value = sample.temperature_celsius();
                rt.air_temp.has_value = true;
                rt.air_temp.last_rx = std::chrono::steady_clock::now();
            });

        auto humidity_sub = node->Subscribe<openember::msgs::sensor::v1::ScalarSample>(
            agri_demo::kAirHumidityTopic,
            [&](const openember::msgs::sensor::v1::ScalarSample& sample) {
                std::lock_guard<std::mutex> lock(rt.mu);
                rt.air_humidity.value = sample.value();
                rt.air_humidity.has_value = true;
                rt.air_humidity.last_rx = std::chrono::steady_clock::now();
            });

        auto light_sub = node->Subscribe<openember::msgs::sensor::v1::ScalarSample>(
            agri_demo::kLightTopic,
            [&](const openember::msgs::sensor::v1::ScalarSample& sample) {
                std::lock_guard<std::mutex> lock(rt.mu);
                rt.light.value = sample.value();
                rt.light.has_value = true;
                rt.light.last_rx = std::chrono::steady_clock::now();
            });

        auto soil_sub = node->Subscribe<openember::msgs::sensor::v1::ScalarSample>(
            agri_demo::kSoilMoistureTopic,
            [&](const openember::msgs::sensor::v1::ScalarSample& sample) {
                std::lock_guard<std::mutex> lock(rt.mu);
                rt.soil.value = sample.value();
                rt.soil.has_value = true;
                rt.soil.last_rx = std::chrono::steady_clock::now();
            });

        std::cout << kNodeName << " started as Link " << link_mode
                  << "\n  status " << agri_demo::kStationStatusTopic
                  << "\n  alert  " << agri_demo::kStationAlertTopic
                  << "\n  soil dry threshold < " << agri_demo::kSoilDryThresholdPercent
                  << "%\n"
                  << std::flush;

#if defined(OPENEMBER_AGRI_STATION_GUI) && OPENEMBER_AGRI_STATION_GUI
        const bool want_gui = !cli.no_gui;
        if (want_gui) {
            int qt_argc = argc;
            QApplication qapp(qt_argc, argv);
            AgriStationViewQt view;
            QObject::connect(&view, &AgriStationViewQt::closeRequested, [&]() {
                openember::Shutdown();
                qapp.quit();
            });

            QTimer timer;
            QObject::connect(&timer, &QTimer::timeout, [&]() {
                if (!openember::Ok()) {
                    qapp.quit();
                    return;
                }
                const auto snap = TickOnce(rt, false);
                view.ApplySnapshot(snap, std::chrono::steady_clock::now());
            });
            timer.start(200);
            view.show();
            std::cout << kNodeName << " Qt field console enabled (use --no-gui for headless)\n"
                      << std::flush;
            const int rc = qapp.exec();
            openember::Shutdown();
            return rc == 0 ? 0 : 1;
        }
#else
        if (!cli.no_gui) {
            std::cout << kNodeName
                      << " built without Qt GUI; running headless "
                         "(enable OPENEMBER_AGRI_STATION_GUI)\n"
                      << std::flush;
        }
#endif

        while (openember::Ok()) {
            (void)TickOnce(rt, true);
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }

        openember::Shutdown();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "agri_station_app failed: " << e.what() << std::endl;
        openember::Shutdown();
        return 1;
    }
}
