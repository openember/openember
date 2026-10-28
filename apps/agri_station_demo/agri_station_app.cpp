/*
 * Copyright (c) 2022-2026, OpenEmber Team
 * SPDX-License-Identifier: Apache-2.0
 *
 * Product app for the agri station: subscribe mock sensors, evaluate
 * thresholds, publish station status / alerts and NodeHeartbeat.
 */

#include <chrono>
#include <cstdint>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>

#include "agri_common.hpp"

#include "openember/init.hpp"
#include "openember/msgs/lifecycle/v1/lifecycle.pb.h"
#include "openember/msgs/node/v1/node.pb.h"
#include "openember/msgs/sensor/v1/sensor.pb.h"
#include "openember/node.hpp"

namespace {

constexpr const char* kNodeName = "agri_station_app";
constexpr const char* kHeartbeatTopic = "/nodes/agri_station_app/heartbeat";
constexpr double kSoilDryThreshold = 25.0;
constexpr double kSensorStaleSec = 2.5;

struct SensorReading {
    double value = 0.0;
    bool has_value = false;
    std::chrono::steady_clock::time_point last_rx =
        std::chrono::steady_clock::time_point::min();
};

bool Fresh(const SensorReading& reading, std::chrono::steady_clock::time_point now) {
    if (!reading.has_value) {
        return false;
    }
    const auto age = std::chrono::duration<double>(now - reading.last_rx).count();
    return age <= kSensorStaleSec;
}

}  // namespace

int main(int argc, char** argv) {
    try {
        const auto link = agri_demo::ParseLinkOptions(argc, argv);
        const auto link_mode = agri_demo::InitLink(link);

        auto node = openember::CreateNode(kNodeName);
        auto heartbeat_pub =
            node->Advertise<openember::msgs::node::v1::NodeHeartbeat>(kHeartbeatTopic);
        auto status_pub =
            node->Advertise<openember::msgs::sensor::v1::ScalarSample>(
                agri_demo::kStationStatusTopic);
        auto alert_pub =
            node->Advertise<openember::msgs::sensor::v1::ScalarSample>(
                agri_demo::kStationAlertTopic);

        std::mutex mu;
        SensorReading air_temp;
        SensorReading air_humidity;
        SensorReading light;
        SensorReading soil;

        auto temp_sub = node->Subscribe<openember::msgs::sensor::v1::TemperatureSample>(
            agri_demo::kAirTempTopic,
            [&](const openember::msgs::sensor::v1::TemperatureSample& sample) {
                std::lock_guard<std::mutex> lock(mu);
                air_temp.value = sample.temperature_celsius();
                air_temp.has_value = true;
                air_temp.last_rx = std::chrono::steady_clock::now();
            });

        auto humidity_sub = node->Subscribe<openember::msgs::sensor::v1::ScalarSample>(
            agri_demo::kAirHumidityTopic,
            [&](const openember::msgs::sensor::v1::ScalarSample& sample) {
                std::lock_guard<std::mutex> lock(mu);
                air_humidity.value = sample.value();
                air_humidity.has_value = true;
                air_humidity.last_rx = std::chrono::steady_clock::now();
            });

        auto light_sub = node->Subscribe<openember::msgs::sensor::v1::ScalarSample>(
            agri_demo::kLightTopic,
            [&](const openember::msgs::sensor::v1::ScalarSample& sample) {
                std::lock_guard<std::mutex> lock(mu);
                light.value = sample.value();
                light.has_value = true;
                light.last_rx = std::chrono::steady_clock::now();
            });

        auto soil_sub = node->Subscribe<openember::msgs::sensor::v1::ScalarSample>(
            agri_demo::kSoilMoistureTopic,
            [&](const openember::msgs::sensor::v1::ScalarSample& sample) {
                std::lock_guard<std::mutex> lock(mu);
                soil.value = sample.value();
                soil.has_value = true;
                soil.last_rx = std::chrono::steady_clock::now();
            });

        const auto start = std::chrono::steady_clock::now();
        std::uint64_t sequence = 0;

        std::cout << kNodeName << " started as Link " << link_mode
                  << "\n  status " << agri_demo::kStationStatusTopic
                  << "\n  alert  " << agri_demo::kStationAlertTopic
                  << "\n  soil dry threshold < " << kSoilDryThreshold << "%\n"
                  << std::flush;

        while (openember::Ok()) {
            const auto now = std::chrono::steady_clock::now();
            SensorReading temp_snap;
            SensorReading humidity_snap;
            SensorReading light_snap;
            SensorReading soil_snap;
            {
                std::lock_guard<std::mutex> lock(mu);
                temp_snap = air_temp;
                humidity_snap = air_humidity;
                light_snap = light;
                soil_snap = soil;
            }

            const bool climate_ok =
                Fresh(temp_snap, now) && Fresh(humidity_snap, now) && Fresh(light_snap, now);
            const bool soil_ok = Fresh(soil_snap, now);
            const bool soil_dry =
                soil_ok && soil_snap.value < kSoilDryThreshold;

            std::string station_state = "running";
            if (!climate_ok && !soil_ok) {
                station_state = "fault";
            } else if (!climate_ok || !soil_ok) {
                station_state = "degraded";
            } else if (soil_dry) {
                station_state = "alert";
            }

            // Encode state as scalar: 0 fault, 1 degraded, 2 running, 3 alert.
            double state_code = 2.0;
            if (station_state == "fault") {
                state_code = 0.0;
            } else if (station_state == "degraded") {
                state_code = 1.0;
            } else if (station_state == "alert") {
                state_code = 3.0;
            }

            (void)status_pub.Publish(agri_demo::MakeScalar(
                kNodeName,
                "station",
                sequence,
                "station_state",
                state_code,
                "enum"));

            if (soil_dry) {
                (void)alert_pub.Publish(agri_demo::MakeScalar(
                    kNodeName,
                    "station",
                    sequence,
                    "soil_too_dry",
                    soil_snap.value,
                    "%"));
            }

            openember::msgs::node::v1::NodeHeartbeat heartbeat;
            agri_demo::FillHeader(heartbeat.mutable_header(), kNodeName, sequence);
            heartbeat.set_node_name(kNodeName);
            heartbeat.set_instance_id(agri_demo::kInstanceId);
            heartbeat.set_lifecycle_state(
                openember::msgs::lifecycle::v1::LIFECYCLE_STATE_ACTIVE);
            heartbeat.set_health(
                station_state == "fault"
                    ? openember::msgs::common::v1::HEALTH_STATE_ERROR
                    : (station_state == "degraded" || station_state == "alert"
                           ? openember::msgs::common::v1::HEALTH_STATE_WARNING
                           : openember::msgs::common::v1::HEALTH_STATE_OK));
            const auto uptime_ms = static_cast<std::uint64_t>(
                std::chrono::duration_cast<std::chrono::milliseconds>(now - start)
                    .count());
            heartbeat.set_uptime_ms(uptime_ms);
            heartbeat.set_error_count(0);
            heartbeat.set_status_message("station_state=" + station_state);

            auto* state_metric = heartbeat.add_metrics();
            state_metric->set_name("agri.station_state");
            state_metric->set_value(state_code);
            state_metric->set_unit("enum");

            if (soil_ok) {
                auto* soil_metric = heartbeat.add_metrics();
                soil_metric->set_name("agri.soil_moisture");
                soil_metric->set_value(soil_snap.value);
                soil_metric->set_unit("%");
            }
            if (temp_snap.has_value && Fresh(temp_snap, now)) {
                auto* temp_metric = heartbeat.add_metrics();
                temp_metric->set_name("agri.air_temperature");
                temp_metric->set_value(temp_snap.value);
                temp_metric->set_unit("C");
            }

            (void)heartbeat_pub.Publish(heartbeat);

            if (sequence % 2 == 0) {
                std::cout << kNodeName << " state=" << station_state
                          << " soil="
                          << (soil_ok ? std::to_string(soil_snap.value) : "stale")
                          << " temp="
                          << (Fresh(temp_snap, now) ? std::to_string(temp_snap.value)
                                                    : "stale")
                          << '\n'
                          << std::flush;
            }

            ++sequence;
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
