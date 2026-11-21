#include "openember/sensor/imu_driver.hpp"
#include "openember/sensor/mock_imu.hpp"
#include <cmath>
#include <stdexcept>
#ifdef OPENEMBER_IMU_DRIVER_YESENSE
#include "openember/sensor/yesense_imu.hpp"
#endif
namespace openember::sensor {
void ImuDriverRegistry::Register(std::string name, Factory factory) {
    if (name.empty() || !factory || !factories_.emplace(std::move(name), std::move(factory)).second)
        throw std::invalid_argument("invalid or duplicate IMU driver registration");
}
std::unique_ptr<IImu> ImuDriverRegistry::Create(const ImuDriverConfig& config) const {
    if (config.sensor_id.empty() || config.frame_id.empty())
        throw std::invalid_argument("sensor_id and frame_id must not be empty");
    auto it = factories_.find(config.driver);
    if (it == factories_.end()) throw std::invalid_argument("IMU driver unavailable in this build: " + config.driver);
    auto driver = it->second(config);
    if (!driver) throw std::runtime_error("IMU factory returned no driver");
    return driver;
}
std::vector<std::string> ImuDriverRegistry::Drivers() const {
    std::vector<std::string> names;
    for (const auto& entry : factories_) names.push_back(entry.first);
    return names;
}
ImuDriverRegistry BuiltinImuDrivers() {
    ImuDriverRegistry registry;
    registry.Register("mock", [](const ImuDriverConfig& c) {
        if (!std::isfinite(c.mock_rate_hz) || c.mock_rate_hz < 1 || c.mock_rate_hz > 4000)
            throw std::invalid_argument("mock rate must be 1..4000 Hz");
        if (!c.device.empty()) throw std::invalid_argument("mock IMU does not use a serial device");
        return std::make_unique<MockImu>(MockImuOptions{c.sensor_id, c.frame_id, c.mock_rate_hz});
    });
#ifdef OPENEMBER_IMU_DRIVER_YESENSE
    registry.Register("yesense", [](const ImuDriverConfig& c) {
        if (c.device.empty() || c.device.front() != '/')
            throw std::invalid_argument("Yesense requires an explicit absolute device path");
        if ((!c.model.empty() && c.model != "YIS321") || c.rx_poll_wait_ms < 1 || c.rx_poll_wait_ms > 10)
            throw std::invalid_argument("unsupported Yesense model or rx_poll_wait_ms (1..10)");
        switch (c.baud_rate) {
        case 9600: case 19200: case 38400: case 57600: case 115200:
        case 76800: case 230400: case 460800: case 921600: break;
        default: throw std::invalid_argument("unsupported host UART baud");
        }
        return std::make_unique<YesenseImu>(YesenseImuOptions{c.device, c.baud_rate,
            c.rx_poll_wait_ms, c.sensor_id, c.frame_id, c.model.empty() ? "YIS321" : c.model, c.device_control});
    });
#endif
    return registry;
}
} // namespace openember::sensor
