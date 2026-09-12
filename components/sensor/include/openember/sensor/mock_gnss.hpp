#pragma once

#include <condition_variable>
#include <mutex>

#include "openember/sensor/gnss.hpp"

namespace openember::sensor {

struct MockGnssOptions {
    std::string sensor_id = "gnss0";
    std::string frame_id = "gnss_link";
    double rate_hz = 1.0;
    double latitude_deg = 31.2304;
    double longitude_deg = 121.4737;
    double altitude_m = 10.0;
};

class MockGnss final : public IGnss {
public:
    explicit MockGnss(MockGnssOptions options = {});
    ~MockGnss() override;

    const SensorInfo& Info() const override;
    hardware::DeviceStatus Status() const override;
    hardware::Result<void> Start() override;
    void Stop() noexcept override;
    hardware::Result<GnssFix> ReadFix(std::chrono::milliseconds timeout) override;

private:
    std::chrono::steady_clock::duration Period() const;

    MockGnssOptions options_;
    SensorInfo info_;
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    bool running_ = false;
    std::uint64_t sequence_ = 0;
    std::chrono::steady_clock::time_point next_sample_;
};

}  // namespace openember::sensor
