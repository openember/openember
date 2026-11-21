#pragma once
#include "openember/sensor/imu.hpp"
#include <cmath>
#include <iomanip>
#include <sstream>
namespace sensor_cli {
inline std::string Quote(const std::string& value) {
    std::ostringstream out; out << '"';
    for (unsigned char c : value) {
        if (c == '"' || c == '\\') out << '\\' << c;
        else if (c < 32) out << "\\u" << std::hex << std::setw(4) << std::setfill('0') << unsigned(c) << std::dec;
        else out << c;
    }
    return out.str() + '"';
}
inline std::string SampleJson(const openember::sensor::ImuSample& s) {
    std::ostringstream out; out << std::setprecision(12) << std::boolalpha;
    auto number = [&](double x) { if (std::isfinite(x)) out << x; else out << "null"; };
    auto vector = [&](const openember::sensor::Vector3& v) {
        out << '['; number(v.x); out << ','; number(v.y); out << ','; number(v.z); out << ']';
    };
    out << "{\"sensor_id\":" << Quote(s.sensor_id) << ",\"frame_id\":" << Quote(s.frame_id)
        << ",\"reference_frame_id\":" << Quote(s.reference_frame_id)
        << ",\"mounting_revision\":" << Quote(s.mounting_revision)
        << ",\"clock_domain\":" << Quote(s.clock_domain) << ",\"sequence\":" << s.sequence
        << ",\"sample_time_monotonic_ns\":" << s.sample_time_monotonic_ns
        << ",\"receive_time_monotonic_ns\":" << s.receive_time_monotonic_ns
        << ",\"acceleration_valid\":" << s.acceleration_valid << ",\"acceleration_mps2\":";
    if (s.acceleration_valid) vector(s.acceleration_mps2); else out << "null";
    out << ",\"angular_velocity_valid\":" << s.angular_velocity_valid << ",\"angular_velocity_radps\":";
    if (s.angular_velocity_valid) vector(s.angular_velocity_radps); else out << "null";
    // Preserve raw quaternion for diagnostics even when its reference is unverified.
    out << ",\"orientation_present\":" << (s.orientation_present || s.orientation_valid)
        << ",\"orientation_valid\":" << s.orientation_valid << ",\"raw_quaternion_wxyz\":";
    if (s.orientation_present || s.orientation_valid) {
        out << '['; number(s.orientation.w); out << ','; number(s.orientation.x); out << ',';
        number(s.orientation.y); out << ','; number(s.orientation.z); out << ']';
    } else out << "null";
    out << ",\"magnetic_field_valid\":" << s.magnetic_field_valid << ",\"magnetic_field_tesla\":";
    if (s.magnetic_field_valid) vector(s.magnetic_field_tesla); else out << "null";
    out << ",\"temperature_valid\":" << s.temperature_valid << ",\"temperature_celsius\":";
    if (s.temperature_valid) number(s.temperature_celsius); else out << "null";
    return out.str() + '}';
}
} // namespace sensor_cli
