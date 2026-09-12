#include "openember/sensor/mock_imu.hpp"

#include <algorithm>
#include <utility>

#include "openember/hardware/timestamp.hpp"

namespace openember::sensor {

MockImu::MockImu(MockImuOptions options)
    : options_(std::move(options)) {
    info_.sensor_id = options_.sensor_id;
    info_.name = options_.sensor_id;
    info_.type = SensorType::kImu;
    info_.vendor = "OpenEmber";
    info_.model = "Mock IMU";
    info_.driver = "mock_imu";
    info_.frame_id = options_.frame_id;
    info_.bus_type = SensorBusType::kVirtual;
    info_.fetch_mode = SensorFetchMode::kPolling;
    info_.min_period_ms = 1000.0 / std::max(options_.rate_hz, 0.001);
}

MockImu::~MockImu() {
    Stop();
}

const SensorInfo& MockImu::Info() const {
    return info_;
}

hardware::DeviceStatus MockImu::Status() const {
    std::lock_guard<std::mutex> lock(mutex_);
    hardware::DeviceStatus status;
    status.availability = running_ ? hardware::DeviceAvailability::kOnline
                                   : hardware::DeviceAvailability::kOffline;
    status.health = running_ ? hardware::HealthState::kOk
                             : hardware::HealthState::kStale;
    status.sequence = sequence_;
    status.message = running_ ? "mock IMU running" : "mock IMU stopped";
    return status;
}

hardware::Result<void> MockImu::Start() {
    std::lock_guard<std::mutex> lock(mutex_);
    running_ = true;
    next_sample_ = std::chrono::steady_clock::now();
    cv_.notify_all();
    return hardware::Result<void>::Success();
}

void MockImu::Stop() noexcept {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        running_ = false;
    }
    cv_.notify_all();
}

hardware::Result<ImuSample> MockImu::ReadSample(std::chrono::milliseconds timeout) {
    std::unique_lock<std::mutex> lock(mutex_);
    if (!running_) {
        return hardware::Result<ImuSample>::Failure(
            hardware::MakeError(hardware::ErrorCode::kNotRunning, "mock IMU is not running"));
    }

    const auto now = std::chrono::steady_clock::now();
    if (now < next_sample_) {
        const auto wait_until = std::min(next_sample_, now + timeout);
        cv_.wait_until(lock, wait_until, [&] { return !running_; });
        if (!running_) {
            return hardware::Result<ImuSample>::Failure(
                hardware::MakeError(hardware::ErrorCode::kNotRunning, "mock IMU stopped"));
        }
        if (std::chrono::steady_clock::now() < next_sample_) {
            return hardware::Result<ImuSample>::Failure(
                hardware::MakeError(hardware::ErrorCode::kTimeout, "mock IMU read timeout"));
        }
    }

    ImuSample sample;
    sample.sensor_id = options_.sensor_id;
    sample.frame_id = options_.frame_id;
    sample.sample_time_monotonic_ns = hardware::SteadyTimeNs();
    sample.sequence = sequence_++;
    sample.acceleration_mps2 = {0.0, 0.0, 9.80665};
    sample.angular_velocity_radps = {0.0, 0.0, 0.0};
    sample.magnetic_field_tesla = {0.0, 0.0, 0.0};
    sample.temperature_celsius = options_.temperature_celsius;
    next_sample_ = std::chrono::steady_clock::now() + Period();
    return hardware::Result<ImuSample>::Success(sample);
}

std::chrono::steady_clock::duration MockImu::Period() const {
    const auto hz = std::max(options_.rate_hz, 0.001);
    return std::chrono::duration_cast<std::chrono::steady_clock::duration>(
        std::chrono::duration<double>(1.0 / hz));
}

}  // namespace openember::sensor
