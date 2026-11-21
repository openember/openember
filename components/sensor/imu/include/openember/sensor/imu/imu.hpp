#pragma once

#include <chrono>
#include <cstdint>
#include <string>

#include "openember/hardware/result.hpp"
#include "openember/sensor/sensor.hpp"
#include "openember/sensor/types.hpp"

namespace openember::sensor {

struct ImuSample {
    std::string sensor_id;
    std::string frame_id;
    std::uint64_t sample_time_monotonic_ns = 0;
    std::uint64_t sequence = 0;
    Vector3 acceleration_mps2;
    Vector3 angular_velocity_radps;
    Vector3 magnetic_field_tesla;
    double temperature_celsius = 0.0;
    // R_reference_sensor rotates vectors in frame_id into reference_frame_id.
    Quaternion orientation;
    bool orientation_valid = false;
    std::string reference_frame_id;
    std::string clock_domain; // Unique producer/acquisition session, not a wall clock.
    std::string mounting_revision;
    std::uint64_t receive_time_monotonic_ns = 0;
    bool acceleration_valid = false;
    bool angular_velocity_valid = false;
    bool magnetic_field_valid = false;
    bool temperature_valid = false;
    // Raw quaternion can be present while reference/mounting remains unverified.
    bool orientation_present = false;
};

class IImu : public ISensor {
public:
    ~IImu() override = default;

    virtual hardware::Result<ImuSample> ReadSample(
        std::chrono::milliseconds timeout) = 0;
};

}  // namespace openember::sensor
