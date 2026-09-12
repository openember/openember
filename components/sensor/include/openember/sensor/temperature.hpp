#pragma once

#include <chrono>
#include <cstdint>
#include <string>

#include "openember/hardware/result.hpp"
#include "openember/sensor/sensor.hpp"

namespace openember::sensor {

struct TemperatureSample {
    std::string sensor_id;
    std::string frame_id;
    std::uint64_t sample_time_monotonic_ns = 0;
    std::uint64_t sequence = 0;
    double temperature_celsius = 0.0;
};

class ITemperatureSensor : public ISensor {
public:
    ~ITemperatureSensor() override = default;

    virtual hardware::Result<TemperatureSample> ReadSample(
        std::chrono::milliseconds timeout) = 0;
};

}  // namespace openember::sensor
