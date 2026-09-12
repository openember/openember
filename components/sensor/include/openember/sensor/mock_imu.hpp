#pragma once

#include <condition_variable>
#include <mutex>

#include "openember/sensor/imu.hpp"

namespace openember::sensor {

struct MockImuOptions {
    std::string sensor_id = "imu0";
    std::string frame_id = "imu_link";
    double rate_hz = 100.0;
    double temperature_celsius = 25.0;
};

class MockImu final : public IImu {
public:
    explicit MockImu(MockImuOptions options = {});
    ~MockImu() override;

    const SensorInfo& Info() const override;
    hardware::DeviceStatus Status() const override;
    hardware::Result<void> Start() override;
    void Stop() noexcept override;
    hardware::Result<ImuSample> ReadSample(std::chrono::milliseconds timeout) override;

private:
    std::chrono::steady_clock::duration Period() const;

    MockImuOptions options_;
    SensorInfo info_;
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    bool running_ = false;
    std::uint64_t sequence_ = 0;
    std::chrono::steady_clock::time_point next_sample_;
};

}  // namespace openember::sensor
