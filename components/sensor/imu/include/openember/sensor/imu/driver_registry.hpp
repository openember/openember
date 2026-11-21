#pragma once
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include "openember/sensor/imu/imu.hpp"
#include "openember/sensor/diagnostics.hpp"
#include "openember/sensor/imu/installation.hpp"

namespace openember::sensor {
// Host acquisition configuration; never changes persistent device parameters.
struct ImuDriverConfig {
    std::string driver = "mock", sensor_id = "imu0", frame_id = "imu_link";
    std::string model, device;
    std::uint32_t baud_rate = 921600, rx_poll_wait_ms = 5;
    double mock_rate_hz = 100;
    bool device_control = false;
    // Raw driver creation does not apply this host-side transform. HI/motion
    // assembly explicitly applies it to samples and preserves raw diagnostics.
    ImuInstallation installation;
};
// Register factories at assembly time, before concurrent acquisition starts.
class ImuDriverRegistry {
public:
    using Factory = std::function<std::unique_ptr<IImu>(const ImuDriverConfig&)>;
    void Register(std::string name, Factory factory);
    std::unique_ptr<IImu> Create(const ImuDriverConfig& config) const;
    std::vector<std::string> Drivers() const;
private:
    std::map<std::string, Factory> factories_;
};
ImuDriverRegistry BuiltinImuDrivers();
} // namespace openember::sensor
