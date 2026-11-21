#include "openember/sensor/mock_imu.hpp"

#include <algorithm>
#include <utility>
#include <random>
#include <cmath>

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
    if (running_) return hardware::Result<void>::Success();
    clock_domain_ = "mock-imu-" + std::to_string(hardware::SteadyTimeNs()) + "-" + std::to_string(std::random_device{}());
    sequence_ = 0;
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

    if (!output_fields_) {
        cv_.wait_for(lock, timeout, [&] { return !running_ || output_fields_ != 0; });
        if (!running_) return hardware::Result<ImuSample>::Failure(hardware::MakeError(hardware::ErrorCode::kNotRunning,"mock IMU stopped"));
        if (!output_fields_) return hardware::Result<ImuSample>::Failure(hardware::MakeError(hardware::ErrorCode::kTimeout,"mock output is disabled"));
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
    sample.receive_time_monotonic_ns = sample.sample_time_monotonic_ns;
    sample.clock_domain = clock_domain_;
    sample.acceleration_mps2 = {0.0, 0.0, 9.80665};
    sample.angular_velocity_radps = {0.0, 0.0, 0.0};
    sample.magnetic_field_tesla = {0.0, 0.0, 0.0};
    sample.temperature_celsius = options_.temperature_celsius;
    sample.acceleration_valid = (output_fields_ & 1) != 0;
    sample.angular_velocity_valid = (output_fields_ & 2) != 0;
    sample.temperature_valid = (output_fields_ & 8) != 0;
    // This legacy fixture has no attitude estimator or magnetic measurement.
    // Keep orientation_valid and magnetic_field_valid false, never fake identity.
    next_sample_ = std::chrono::steady_clock::now() + Period();
    return hardware::Result<ImuSample>::Success(sample);
}

std::chrono::steady_clock::duration MockImu::Period() const {
    const auto hz = std::max(options_.rate_hz, 0.001);
    return std::chrono::duration_cast<std::chrono::steady_clock::duration>(
        std::chrono::duration<double>(1.0 / hz));
}


ImuCapabilities MockImu::Capabilities() const {
    ImuCapabilities c;
    c.configuration_access = true;
    c.output_rates_hz = {1,2,5,10,20,25,50,100,200,400,800,1600};
    c.output_fields = 1|2|8;
    c.persistence = {ImuPersistence::kRam};
    c.static_gyro_calibration = true;
    c.profile = "Mock IMU; calibration is simulated; no UART or Flash";
    return c;
}
hardware::Result<ImuDeviceConfiguration> MockImu::ReadConfiguration() {
    std::lock_guard<std::mutex> lock(mutex_);
    ImuDeviceConfiguration c;
    c.output_rate_hz = options_.rate_hz;
    c.output_fields = output_fields_;
    c.queried_at_monotonic_ns = hardware::SteadyTimeNs();
    return hardware::Result<ImuDeviceConfiguration>::Success(c);
}
hardware::Result<ImuConfigurationResult> MockImu::ApplyConfiguration(const ImuConfigurationPatch& patch) {
    using Result = hardware::Result<ImuConfigurationResult>;
    std::lock_guard<std::mutex> lock(mutex_);
    if (!running_) return Result::Failure(hardware::MakeError(hardware::ErrorCode::kNotRunning,"Mock IMU is closed"));
    const auto rates = Capabilities().output_rates_hz;
    if (patch.persistence != ImuPersistence::kRam || patch.baud_rate ||
        (patch.output_fields && (*patch.output_fields & ~(1u|2u|8u))) ||
        (patch.output_rate_hz && std::find(rates.begin(),rates.end(),*patch.output_rate_hz) == rates.end()) ||
        (!patch.output_rate_hz && !patch.output_fields))
        return Result::Failure(hardware::MakeError(hardware::ErrorCode::kInvalidConfig,"unsupported or empty Mock configuration"));
    ImuConfigurationResult result;
    if (patch.output_rate_hz) {
        options_.rate_hz = *patch.output_rate_hz;
        result.settings.push_back({"rate",ImuSettingState::kApplied,"Mock rate applied in RAM"});
    }
    if (patch.output_fields) {
        output_fields_ = *patch.output_fields;
        result.settings.push_back({"fields",ImuSettingState::kApplied,"Mock output fields applied in RAM"});
    }
    ImuDeviceConfiguration observed;
    observed.output_rate_hz = options_.rate_hz;
    observed.output_fields = output_fields_;
    observed.queried_at_monotonic_ns = hardware::SteadyTimeNs();
    result.observed = observed;
    next_sample_ = std::chrono::steady_clock::now(); cv_.notify_all();
    return Result::Success(std::move(result));
}
hardware::Result<ImuCalibrationResult> MockImu::CalibrateStaticGyro(const ImuCalibrationRequest& request) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!running_ || request.persistence != ImuPersistence::kRam)
        return hardware::Result<ImuCalibrationResult>::Failure(hardware::MakeError(hardware::ErrorCode::kNotSupported,"Mock calibration requires running RAM session"));
    return hardware::Result<ImuCalibrationResult>::Success({ImuCalibrationState::kCompleted,"simulated static gyro calibration; no physical device changed"});
}
}  // namespace openember::sensor
