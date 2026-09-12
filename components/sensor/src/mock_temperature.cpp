#include "openember/sensor/mock_temperature.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

#include "openember/hardware/timestamp.hpp"

namespace openember::sensor {

MockTemperature::MockTemperature(MockTemperatureOptions options)
    : options_(std::move(options)) {
    info_.sensor_id = options_.sensor_id;
    info_.name = options_.sensor_id;
    info_.type = SensorType::kTemperature;
    info_.vendor = "OpenEmber";
    info_.model = "Mock Temperature";
    info_.driver = "mock_temperature";
    info_.frame_id = options_.frame_id;
    info_.bus_type = SensorBusType::kVirtual;
    info_.fetch_mode = SensorFetchMode::kPolling;
    info_.min_period_ms = 1000.0 / std::max(options_.rate_hz, 0.001);
}

MockTemperature::~MockTemperature() {
    Stop();
}

const SensorInfo& MockTemperature::Info() const {
    return info_;
}

hardware::DeviceStatus MockTemperature::Status() const {
    std::lock_guard<std::mutex> lock(mutex_);
    hardware::DeviceStatus status;
    status.availability = running_ ? hardware::DeviceAvailability::kOnline
                                   : hardware::DeviceAvailability::kOffline;
    status.health = running_ ? hardware::HealthState::kOk
                             : hardware::HealthState::kStale;
    status.sequence = sequence_;
    status.message = running_ ? "mock temperature running" : "mock temperature stopped";
    return status;
}

hardware::Result<void> MockTemperature::Start() {
    std::lock_guard<std::mutex> lock(mutex_);
    running_ = true;
    next_sample_ = std::chrono::steady_clock::now();
    cv_.notify_all();
    return hardware::Result<void>::Success();
}

void MockTemperature::Stop() noexcept {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        running_ = false;
    }
    cv_.notify_all();
}

hardware::Result<TemperatureSample> MockTemperature::ReadSample(
    std::chrono::milliseconds timeout) {
    std::unique_lock<std::mutex> lock(mutex_);
    if (!running_) {
        return hardware::Result<TemperatureSample>::Failure(
            hardware::MakeError(hardware::ErrorCode::kNotRunning, "mock temperature is not running"));
    }

    const auto now = std::chrono::steady_clock::now();
    if (now < next_sample_) {
        const auto wait_until = std::min(next_sample_, now + timeout);
        cv_.wait_until(lock, wait_until, [&] { return !running_; });
        if (!running_) {
            return hardware::Result<TemperatureSample>::Failure(
                hardware::MakeError(hardware::ErrorCode::kNotRunning, "mock temperature stopped"));
        }
        if (std::chrono::steady_clock::now() < next_sample_) {
            return hardware::Result<TemperatureSample>::Failure(
                hardware::MakeError(hardware::ErrorCode::kTimeout, "mock temperature read timeout"));
        }
    }

    TemperatureSample sample;
    sample.sensor_id = options_.sensor_id;
    sample.frame_id = options_.frame_id;
    sample.sample_time_monotonic_ns = hardware::SteadyTimeNs();
    sample.sequence = sequence_++;
    sample.temperature_celsius =
        options_.initial_temperature_celsius + 0.2 * std::sin(static_cast<double>(sample.sequence) * 0.1);
    next_sample_ = std::chrono::steady_clock::now() + Period();
    return hardware::Result<TemperatureSample>::Success(sample);
}

std::chrono::steady_clock::duration MockTemperature::Period() const {
    const auto hz = std::max(options_.rate_hz, 0.001);
    return std::chrono::duration_cast<std::chrono::steady_clock::duration>(
        std::chrono::duration<double>(1.0 / hz));
}

}  // namespace openember::sensor
