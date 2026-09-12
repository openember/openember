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
};

class IImu : public ISensor {
public:
    ~IImu() override = default;

    virtual hardware::Result<ImuSample> ReadSample(
        std::chrono::milliseconds timeout) = 0;
};

}  // namespace openember::sensor
