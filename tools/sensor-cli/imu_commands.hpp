#pragma once
#include "openember/sensor/imu/calibration.hpp"
#include "sample_json.hpp"
#include <iostream>
#include <sstream>
#include <stdexcept>
namespace sensor_cli {
struct FieldName { const char* name; unsigned bit; };
inline constexpr FieldName kFields[] = {{"accel",1},{"gyro",2},{"quaternion",4},{"temperature",8},{"magnetic",16},{"euler",32},{"timestamps",64}};
inline std::string Fields(unsigned mask) {
    std::string result;
    for (const auto& field : kFields) if (mask&field.bit) { if (!result.empty()) result += ','; result += field.name; }
    return result.empty() ? "none" : result;
}
inline unsigned ParseFields(const std::string& value) {
    if (value == "none") return 0;
    if (value.empty() || value.back() == ',') throw std::invalid_argument("empty output field");
    unsigned fields = 0;
    std::istringstream input(value); std::string name;
    while (std::getline(input,name,',')) {
        bool found = false;
        for (const auto& field : kFields) if (name == field.name) {
            if (fields&field.bit) throw std::invalid_argument("duplicate output field: " + name);
            fields |= field.bit; found = true;
        }
        if (!found) throw std::invalid_argument("unknown output field: " + name);
    }
    return fields;
}
inline void PrintConfiguration(const openember::sensor::ImuDeviceConfiguration& config) {
    std::cout << "DEVICE_CONFIG rate=" << (config.output_rate_hz ? std::to_string(*config.output_rate_hz) : "unknown")
        << " fields=" << (config.output_fields ? Fields(*config.output_fields) : "unknown")
        << " device_baud=" << (config.baud_rate ? std::to_string(*config.baud_rate) : "unknown")
        << " host_baud=" << config.host_baud_rate << " queried_at_ns=" << config.queried_at_monotonic_ns
        << " persistence=unknown\n";
}
inline const char* SettingState(openember::sensor::ImuSettingState state) {
    using State = openember::sensor::ImuSettingState;
    return state == State::kApplied ? "APPLIED" : state == State::kRejected ? "REJECTED" : "UNCONFIRMED";
}
} // namespace sensor_cli
