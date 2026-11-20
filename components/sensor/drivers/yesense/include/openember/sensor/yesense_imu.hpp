#pragma once

#include <memory>
#include "openember/sensor/imu.hpp"

namespace openember::sensor {

struct ImuReadDiagnostics {
    std::uint64_t bytes = 0, decoded = 0, accepted = 0;
    std::uint64_t rejected_tid = 0, rejected_values = 0, timeouts = 0;
    std::uint64_t drain_limit_hits = 0;
    std::uint32_t host_baud = 0;
    bool host_settings_restored = false;
};

// Diagnostic-only raw sensor frame. No device configuration, automatic reconnect,
// installation transform, or controller-ready orientation. One reading thread;
// Stop can cancel it. Serialize lifecycle Start/Stop calls at the owner.
struct YesenseImuOptions {
    std::string device;
    // Zero preserves the current host baud (legacy diagnostic constructor).
    // A nonzero value configures only the host UART, never the IMU itself.
    std::uint32_t baud_rate = 0;
    std::uint32_t rx_poll_wait_ms = 10;
    std::string sensor_id = "yesense0";
    std::string frame_id = "yesense_sensor_unverified";
    std::string model = "YIS321";
};

class YesenseImu final : public openember::sensor::IImu {
public:
    explicit YesenseImu(std::string device);
    explicit YesenseImu(YesenseImuOptions options);
    ~YesenseImu() override;
    const openember::sensor::SensorInfo& Info() const override;
    openember::hardware::DeviceStatus Status() const override;
    openember::hardware::Result<void> Start() override;
    void Stop() noexcept override;
    openember::hardware::Result<openember::sensor::ImuSample> ReadSample(
        std::chrono::milliseconds timeout) override;
    ImuReadDiagnostics Diagnostics() const;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace openember::sensor
