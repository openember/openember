#include "openember/sensor/mock_gnss.hpp"

#include <algorithm>
#include <utility>

#include "openember/hardware/timestamp.hpp"

namespace openember::sensor {

MockGnss::MockGnss(MockGnssOptions options)
    : options_(std::move(options)) {
    info_.sensor_id = options_.sensor_id;
    info_.name = options_.sensor_id;
    info_.type = SensorType::kGnss;
    info_.vendor = "OpenEmber";
    info_.model = "Mock GNSS";
    info_.driver = "mock_gnss";
    info_.frame_id = options_.frame_id;
    info_.bus_type = SensorBusType::kVirtual;
    info_.fetch_mode = SensorFetchMode::kPolling;
    info_.min_period_ms = 1000.0 / std::max(options_.rate_hz, 0.001);
}

MockGnss::~MockGnss() {
    Stop();
}

const SensorInfo& MockGnss::Info() const {
    return info_;
}

hardware::DeviceStatus MockGnss::Status() const {
    std::lock_guard<std::mutex> lock(mutex_);
    hardware::DeviceStatus status;
    status.availability = running_ ? hardware::DeviceAvailability::kOnline
                                   : hardware::DeviceAvailability::kOffline;
    status.health = running_ ? hardware::HealthState::kOk
                             : hardware::HealthState::kStale;
    status.sequence = sequence_;
    status.message = running_ ? "mock GNSS running" : "mock GNSS stopped";
    return status;
}

hardware::Result<void> MockGnss::Start() {
    std::lock_guard<std::mutex> lock(mutex_);
    running_ = true;
    next_sample_ = std::chrono::steady_clock::now();
    cv_.notify_all();
    return hardware::Result<void>::Success();
}

void MockGnss::Stop() noexcept {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        running_ = false;
    }
    cv_.notify_all();
}

hardware::Result<GnssFix> MockGnss::ReadFix(std::chrono::milliseconds timeout) {
    std::unique_lock<std::mutex> lock(mutex_);
    if (!running_) {
        return hardware::Result<GnssFix>::Failure(
            hardware::MakeError(hardware::ErrorCode::kNotRunning, "mock GNSS is not running"));
    }

    const auto now = std::chrono::steady_clock::now();
    if (now < next_sample_) {
        const auto wait_until = std::min(next_sample_, now + timeout);
        cv_.wait_until(lock, wait_until, [&] { return !running_; });
        if (!running_) {
            return hardware::Result<GnssFix>::Failure(
                hardware::MakeError(hardware::ErrorCode::kNotRunning, "mock GNSS stopped"));
        }
        if (std::chrono::steady_clock::now() < next_sample_) {
            return hardware::Result<GnssFix>::Failure(
                hardware::MakeError(hardware::ErrorCode::kTimeout, "mock GNSS read timeout"));
        }
    }

    GnssFix fix;
    fix.sensor_id = options_.sensor_id;
    fix.frame_id = options_.frame_id;
    fix.sample_time_monotonic_ns = hardware::SteadyTimeNs();
    fix.sequence = sequence_++;
    fix.fix_type = GnssFixType::kFix3D;
    fix.latitude_deg = options_.latitude_deg;
    fix.longitude_deg = options_.longitude_deg;
    fix.altitude_m = options_.altitude_m;
    fix.velocity_enu_mps = {0.0, 0.0, 0.0};
    fix.satellites_used = 12;
    fix.horizontal_accuracy_m = 0.8;
    fix.vertical_accuracy_m = 1.2;
    next_sample_ = std::chrono::steady_clock::now() + Period();
    return hardware::Result<GnssFix>::Success(fix);
}

std::chrono::steady_clock::duration MockGnss::Period() const {
    const auto hz = std::max(options_.rate_hz, 0.001);
    return std::chrono::duration_cast<std::chrono::steady_clock::duration>(
        std::chrono::duration<double>(1.0 / hz));
}

}  // namespace openember::sensor
