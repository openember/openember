#pragma once

#include <condition_variable>
#include <mutex>

#include "openember/sensor/temperature.hpp"

namespace openember::sensor {

struct MockTemperatureOptions {
    std::string sensor_id = "temp0";
    std::string frame_id = "temperature_link";
    double rate_hz = 2.0;
    double initial_temperature_celsius = 25.0;
};

class MockTemperature final : public ITemperatureSensor {
public:
    explicit MockTemperature(MockTemperatureOptions options = {});
    ~MockTemperature() override;

    const SensorInfo& Info() const override;
    hardware::DeviceStatus Status() const override;
    hardware::Result<void> Start() override;
    void Stop() noexcept override;
    hardware::Result<TemperatureSample> ReadSample(
        std::chrono::milliseconds timeout) override;

private:
    std::chrono::steady_clock::duration Period() const;

    MockTemperatureOptions options_;
    SensorInfo info_;
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    bool running_ = false;
    std::uint64_t sequence_ = 0;
    std::chrono::steady_clock::time_point next_sample_;
};

}  // namespace openember::sensor
