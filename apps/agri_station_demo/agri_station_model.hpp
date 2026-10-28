/*
 * Copyright (c) 2022-2026, OpenEmber Team
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <chrono>
#include <cstdio>
#include <string>

namespace agri_demo {

constexpr double kSoilDryThresholdPercent = 25.0;
constexpr double kSensorStaleSec = 2.5;

struct SensorReading {
    double value = 0.0;
    bool has_value = false;
    std::chrono::steady_clock::time_point last_rx =
        std::chrono::steady_clock::time_point::min();
};

inline bool Fresh(const SensorReading& reading,
                  std::chrono::steady_clock::time_point now) {
    if (!reading.has_value) {
        return false;
    }
    const auto age = std::chrono::duration<double>(now - reading.last_rx).count();
    return age <= kSensorStaleSec;
}

struct StationSnapshot {
    std::string state = "fault";  // fault | degraded | running | alert
    double state_code = 0.0;
    bool climate_ok = false;
    bool soil_ok = false;
    bool soil_dry = false;
    SensorReading temperature;
    SensorReading humidity;
    SensorReading light;
    SensorReading soil;
};

inline StationSnapshot EvaluateStation(const SensorReading& temperature,
                                       const SensorReading& humidity,
                                       const SensorReading& light,
                                       const SensorReading& soil,
                                       std::chrono::steady_clock::time_point now) {
    StationSnapshot snap;
    snap.temperature = temperature;
    snap.humidity = humidity;
    snap.light = light;
    snap.soil = soil;
    snap.climate_ok =
        Fresh(temperature, now) && Fresh(humidity, now) && Fresh(light, now);
    snap.soil_ok = Fresh(soil, now);
    snap.soil_dry = snap.soil_ok && soil.value < kSoilDryThresholdPercent;

    snap.state = "running";
    snap.state_code = 2.0;
    if (!snap.climate_ok && !snap.soil_ok) {
        snap.state = "fault";
        snap.state_code = 0.0;
    } else if (!snap.climate_ok || !snap.soil_ok) {
        snap.state = "degraded";
        snap.state_code = 1.0;
    } else if (snap.soil_dry) {
        snap.state = "alert";
        snap.state_code = 3.0;
    }
    return snap;
}

inline std::string FormatReading(const SensorReading& reading,
                                 std::chrono::steady_clock::time_point now,
                                 int precision = 1) {
    if (!Fresh(reading, now)) {
        return "—";
    }
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%.*f", precision, reading.value);
    return buf;
}

}  // namespace agri_demo
