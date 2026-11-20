#include "lpio/SerialPort.hpp" // Before termios baud-rate macros.
#include "openember/sensor/yesense_imu.hpp"
#include "tid_progress.hpp"

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <cmath>
#include <cstring>
#include <fcntl.h>
#include <mutex>
#include <random>
#include <stdexcept>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

#include "yesense_decoder.hpp"
#include <poll.h>
#include <array>
#include "openember/hardware/timestamp.hpp"

namespace openember::sensor {
namespace hw = openember::hardware;
namespace sensor = openember::sensor;
namespace {
using Clock = std::chrono::steady_clock;

std::uint32_t HostBaud(const termios& settings) {
    const auto speed = cfgetospeed(&settings);
    if (cfgetispeed(&settings) != speed)
        throw std::runtime_error("different input/output baud rates are unsupported");
    switch (speed) {
    case B9600: return 9600;
    case B19200: return 19200;
    case B38400: return 38400;
    case B57600: return 57600;
#ifdef B76800
    case B76800: return 76800;
#endif
    case B115200: return 115200;
    case B230400: return 230400;
    case B460800: return 460800;
    case B921600: return 921600;
    default: throw std::runtime_error("current host baud is unsupported; refusing to change it");
    }
}

void Check(bool ok, const char* operation) {
    if (!ok) throw std::runtime_error(std::string(operation) + ": " + std::strerror(errno));
}
} // namespace

struct YesenseImu::Impl {
    explicit Impl(YesenseImuOptions value) : options(std::move(value)), device(options.device) {
        info.sensor_id = options.sensor_id;
        info.model = options.model;
        info.name = "YESENSE diagnostic IMU";
        info.type = sensor::SensorType::kImu;
        info.vendor = "YESENSE";
        info.driver = "yesense_readonly";
        info.frame_id = options.frame_id;
        info.bus_type = sensor::SensorBusType::kSerial;
        info.fetch_mode = sensor::SensorFetchMode::kPolling;
    }
    void Cleanup() noexcept {
        serial.reset();
        decoder = {};
        if (guard >= 0) {
            // Keep the guard open until termios and exclusivity have been restored.
            diagnostics.host_settings_restored = saved && tcsetattr(guard, TCSANOW, &original) == 0;
            if (exclusive) ioctl(guard, TIOCNXCL);
            close(guard);
            guard = -1;
        }
        saved = exclusive = false;
    }
    void Error(hw::ErrorCode code, const std::string& message) {
        status.last_error = hw::MakeError(code, message);
        status.message = message;
        ++status.error_count;
        status.health = code == hw::ErrorCode::kTimeout ? hw::HealthState::kStale : hw::HealthState::kError;
        status.availability = code == hw::ErrorCode::kTimeout ? hw::DeviceAvailability::kDegraded : hw::DeviceAvailability::kError;
    }
    YesenseImuOptions options;
    std::string device, domain;
    sensor::SensorInfo info;
    mutable std::mutex mutex;
    std::atomic_bool stopped{true};
    std::unique_ptr<lpio::SerialPort> serial;
    detail::YesenseDecoder decoder;
    int guard = -1;
    termios original{};
    bool saved = false, exclusive = false, have_tid = false;
    std::uint16_t tid = 0;
    std::uint64_t last_receive = 0;
    hw::DeviceStatus status;
    ImuReadDiagnostics diagnostics;
};

YesenseImu::YesenseImu(std::string device) : YesenseImu(YesenseImuOptions{std::move(device)}) {}
YesenseImu::YesenseImu(YesenseImuOptions options) : impl_(std::make_unique<Impl>(std::move(options))) {}
YesenseImu::~YesenseImu() { Stop(); }
const sensor::SensorInfo& YesenseImu::Info() const { return impl_->info; }
hw::DeviceStatus YesenseImu::Status() const {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    return impl_->status;
}
ImuReadDiagnostics YesenseImu::Diagnostics() const {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    return impl_->diagnostics;
}

hw::Result<void> YesenseImu::Start() {
    auto& p = *impl_;
    std::lock_guard<std::mutex> lock(p.mutex);
    if (!p.stopped) return hw::Result<void>::Success();
    p.Cleanup();
    p.diagnostics = {};
    p.status = {};
    try {
        if (p.device.empty() || p.device.front() != '/')
            throw std::runtime_error("an explicit absolute serial device path is required");
        if (p.options.rx_poll_wait_ms < 1 || p.options.rx_poll_wait_ms > 10 ||
            p.options.sensor_id.empty() || p.options.frame_id.empty() || p.options.model != "YIS321")
            throw std::runtime_error("invalid Yesense identity/model or rx_poll_wait_ms (1..10)");
        switch (p.options.baud_rate) {
        case 0: case 9600: case 19200: case 38400: case 57600: case 115200:
        case 230400: case 460800: case 921600: break;
        default: throw std::runtime_error("unsupported host UART baud_rate");
        }
        p.guard = open(p.device.c_str(), O_RDONLY | O_NOCTTY | O_NONBLOCK | O_CLOEXEC);
        Check(p.guard >= 0, "open serial guard");
        Check(flock(p.guard, LOCK_EX | LOCK_NB) == 0, "lock serial device");
        Check(tcgetattr(p.guard, &p.original) == 0, "save host termios");
        p.saved = true;
        p.diagnostics.host_baud = p.options.baud_rate ? p.options.baud_rate : HostBaud(p.original);
        lpio::SerialPortConfig config;
        config.baudRate = p.diagnostics.host_baud;
        config.nonBlocking = true;
        p.serial = std::make_unique<lpio::SerialPort>(p.device, config);
        // Open before TIOCEXCL; guard retains the original host settings for Stop.
        p.serial->open();
        Check(tcflush(p.guard, TCIFLUSH) == 0, "flush serial input");
        Check(ioctl(p.guard, TIOCEXCL) == 0, "make serial device exclusive");
        p.exclusive = true;
        p.domain = "yesense-" + std::to_string(hw::SteadyTimeNs()) + "-" + std::to_string(std::random_device{}());
        p.have_tid = false;
        p.last_receive = 0;
        p.stopped = false;
        p.status.availability = hw::DeviceAvailability::kOnline;
        p.status.health = hw::HealthState::kWarning;
        p.status.message = "diagnostic only: reference/mounting unverified; awaiting data";
        return hw::Result<void>::Success();
    } catch (const std::exception& e) {
        p.Cleanup();
        p.Error(hw::ErrorCode::kOpenFailed, e.what());
        return hw::Result<void>::Failure(p.status.last_error);
    }
}

void YesenseImu::Stop() noexcept {
    auto& p = *impl_;
    p.stopped = true; // Bounded poll observes cancellation before Stop takes the mutex.
    std::lock_guard<std::mutex> lock(p.mutex);
    p.Cleanup();
    p.status.availability = hw::DeviceAvailability::kOffline;
    if (p.status.health != hw::HealthState::kError) p.status.health = hw::HealthState::kStale;
}

hw::Result<sensor::ImuSample> YesenseImu::ReadSample(std::chrono::milliseconds timeout) {
    auto& p = *impl_;
    std::lock_guard<std::mutex> lock(p.mutex);
    using Result = hw::Result<sensor::ImuSample>;
    if (timeout.count() < 0 || timeout > std::chrono::seconds(60))
        return Result::Failure(hw::MakeError(hw::ErrorCode::kInvalidConfig, "read timeout must be 0..60000 ms"));
    const auto deadline = Clock::now() + timeout;
    do {
        if (p.stopped) return Result::Failure(hw::MakeError(hw::ErrorCode::kNotRunning, "IMU stopped"));
        try {
            const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - Clock::now()).count();
            pollfd ready{p.guard, POLLIN, 0};
            const int wait = static_cast<int>(std::clamp<std::int64_t>(remaining, 0, p.options.rx_poll_wait_ms));
            const int polled = poll(&ready, 1, wait);
            if (polled < 0 && errno == EINTR) continue;
            Check(polled >= 0, "poll serial input");
            if (ready.revents & (POLLERR | POLLHUP | POLLNVAL))
                throw std::runtime_error("serial device disconnected");
            detail::YesenseFrame f;
            bool have_frame = false;
            std::array<std::byte, 4096> bytes;
            std::size_t drained = 0;
            while (!p.stopped && (ready.revents & POLLIN)) {
                const auto count = p.serial->read(bytes);
                if (count == 0) break;
                drained += count;
                p.diagnostics.bytes += count;
                const auto decoded = p.decoder.Feed(bytes.data(), count, f);
                p.diagnostics.decoded += decoded;
                have_frame = have_frame || decoded > 0;
                if (drained >= 65536) {
                    ++p.diagnostics.drain_limit_hits;
                    throw std::runtime_error("serial drain budget exceeded; explicit restart required");
                }
            }
            if (p.stopped) return Result::Failure(hw::MakeError(hw::ErrorCode::kNotRunning, "IMU stopped"));
            if (have_frame) {
                if (!detail::IsNewYesenseTid(f.tid, p.have_tid ? p.tid : 0)) {
                    ++p.diagnostics.rejected_tid;
                    continue;
                }
                const double norm2 = f.qw*f.qw + f.qx*f.qx + f.qy*f.qy + f.qz*f.qz;
                if (!std::isfinite(f.ax) || !std::isfinite(f.ay) || !std::isfinite(f.az) ||
                    !std::isfinite(f.wx) || !std::isfinite(f.wy) || !std::isfinite(f.wz) ||
                    !std::isfinite(norm2) || std::abs(norm2 - 1.0) > 0.001) {
                    ++p.diagnostics.rejected_values;
                    continue;
                }
                const auto received = hw::SteadyTimeNs();
                if (received <= p.last_receive) continue;
                sensor::ImuSample sample;
                sample.sensor_id = p.info.sensor_id;
                sample.frame_id = p.info.frame_id;
                sample.reference_frame_id = "yesense_reference_unverified";
                sample.mounting_revision = "unverified";
                sample.clock_domain = p.domain;
                sample.receive_time_monotonic_ns = received;
                // No verified monotonic device clock mapping is available.
                sample.sequence = p.diagnostics.decoded;
                sample.acceleration_mps2 = {f.ax, f.ay, f.az};
                sample.angular_velocity_radps = {f.wx, f.wy, f.wz};
                sample.orientation = {f.qw, f.qx, f.qy, f.qz};
                sample.acceleration_valid = sample.angular_velocity_valid = true;
                // Quaternion is retained for diagnostics, not declared R_reference_sensor
                // until the device convention is verified. No normalization/axis guessing.
                sample.orientation_valid = false;
                p.have_tid = true;
                p.tid = f.tid;
                p.last_receive = received;
                ++p.diagnostics.accepted;
                p.status.sequence = sample.sequence;
                p.status.availability = hw::DeviceAvailability::kOnline;
                p.status.health = hw::HealthState::kWarning;
                p.status.last_error = {};
                p.status.message = "diagnostic data only; orientation contract unverified";
                return Result::Success(std::move(sample));
            }
        } catch (const std::exception& e) {
            p.stopped = true;
            p.Error(hw::ErrorCode::kIoError, e.what());
            return Result::Failure(p.status.last_error);
        }
    } while (Clock::now() < deadline);
    ++p.diagnostics.timeouts;
    p.Error(hw::ErrorCode::kTimeout, "no new admissible IMU frame before deadline");
    return Result::Failure(p.status.last_error);
}
} // namespace openember::sensor
