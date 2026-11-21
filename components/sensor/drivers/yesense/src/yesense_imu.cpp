#include "lpio/SerialPort.hpp" // Before termios baud-rate macros.
#include "openember/sensor/yesense_imu.hpp"
#include "tid_progress.hpp"
#include "serial_settings.hpp"

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <cmath>
#include <cstring>
#include <fcntl.h>
#include <mutex>
#include <random>
#include <optional>
#include <thread>
#include <deque>
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
        info.driver = options.device_control ? "yesense" : "yesense_readonly";
        info.frame_id = options.frame_id;
        info.bus_type = sensor::SensorBusType::kSerial;
        info.fetch_mode = sensor::SensorFetchMode::kPolling;
    }
    void Cleanup() noexcept {
        serial.reset();
        decoder = {};
        pending.reset();
        replies.clear();
        if (guard >= 0) {
            // Keep the guard open until termios and exclusivity have been restored.
            diagnostics.host_settings_restored = saved && original.Restore(guard);
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
    std::optional<detail::YesenseFrame> pending;
    std::uint64_t pending_receive = 0;
    std::vector<detail::YesenseReply> replies;
    Clock::time_point opened_at{};
    std::deque<std::pair<Clock::time_point, bool>> stationary;
    bool configuration_uncertain = false;
    std::uint16_t last_observed_tid = 0;
    void EnsureControl() const {
        if (!options.device_control) throw std::runtime_error("device control is disabled");
        if (stopped) throw std::runtime_error("device is not running");
        if (configuration_uncertain) throw std::runtime_error("previous transaction unresolved; close and reopen before querying or writing again");
    }
    ImuDeviceConfiguration ReadCurrent();
    void Pump(int wait) {
        if (stopped) throw std::runtime_error("IMU stopped");
        pollfd ready{guard, POLLIN, 0};
        const int polled = poll(&ready, 1, wait);
        if (polled < 0 && errno == EINTR) return;
        Check(polled >= 0, "poll serial input");
        if (ready.revents & (POLLERR | POLLHUP | POLLNVAL)) throw std::runtime_error("serial device disconnected");
        std::array<std::byte, 4096> bytes;
        std::size_t drained = 0;
        while (!stopped && (ready.revents & POLLIN)) {
            const auto count = serial->read(bytes);
            if (!count) break;
            drained += count; diagnostics.bytes += count;
            detail::YesenseFrame frame;
            const auto decoded = decoder.Feed(bytes.data(), count, frame);
            diagnostics.decoded += decoded;
            if (decoded) {
                pending = frame; pending_receive = hw::SteadyTimeNs();
                const double gyro = std::sqrt(frame.wx*frame.wx+frame.wy*frame.wy+frame.wz*frame.wz);
                const double accel = std::sqrt(frame.ax*frame.ax+frame.ay*frame.ay+frame.az*frame.az);
                const bool still = (frame.fields & 3) == 3 && gyro < .05 && accel > 9.3 && accel < 10.3;
                const auto now = Clock::now();
                if (detail::IsNewYesenseTid(frame.tid,last_observed_tid)) {
                    stationary.emplace_back(now, still); last_observed_tid = frame.tid;
                }
                while (stationary.size() > 4096 || (!stationary.empty() && now-stationary.front().first > std::chrono::seconds(2))) stationary.pop_front();
            }
            auto parsed = decoder.TakeReplies();
            for (auto& reply : parsed) {
                if (replies.size() == 32) replies.erase(replies.begin());
                replies.push_back(std::move(reply));
            }
            if (drained >= 65536) { ++diagnostics.drain_limit_hits; throw std::runtime_error("serial drain budget exceeded"); }
        }
    }
    detail::YesenseReply Exchange(unsigned category, unsigned operation,
        const std::vector<std::uint8_t>& payload, int timeout_ms = 500) {
        // Some USB bridges accept host writes before their streaming endpoint is
        // ready. Wait for initial traffic, with a bound that also permits no-output mode.
        const auto ready_deadline = opened_at + std::chrono::milliseconds(200);
        while (!diagnostics.decoded && !stopped && Clock::now() < ready_deadline) Pump(5);
        // Drain existing traffic before arming a new transaction; no tcflush for queries.
        Pump(0); replies.clear();
        const auto command = detail::YesenseDecoder::Command(category, operation, payload);
        const auto deadline = Clock::now() + std::chrono::milliseconds(timeout_ms);
        std::size_t sent = 0;
        while (sent < command.size()) {
            if (stopped || Clock::now() >= deadline) throw std::runtime_error("configuration send timed out/cancelled; result unknown");
            sent += serial->write(std::span<const std::byte>(command).subspan(sent));
            if (sent < command.size()) std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        while (!stopped && Clock::now() < deadline) {
            Pump(5);
            for (const auto& reply : replies) {
                if (reply.category == category && reply.operation == operation) return reply;
            }
            replies.clear();
        }
        configuration_uncertain = true;
        throw std::runtime_error("configuration response timed out; reopen before another transaction");
    }
    int guard = -1;
    detail::SerialSettings original;
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

std::vector<DiagnosticCounter> YesenseImu::ReadDiagnostics() const {
    const auto d = Diagnostics();
    return {{"bytes", d.bytes}, {"decoded", d.decoded}, {"accepted", d.accepted},
        {"rejected_tid", d.rejected_tid}, {"rejected_values", d.rejected_values},
        {"timeouts", d.timeouts}, {"drain_limit_hits", d.drain_limit_hits}};
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
        case 76800: case 230400: case 460800: case 921600: break;
        default: throw std::runtime_error("unsupported host UART baud_rate");
        }
        p.guard = open(p.device.c_str(), O_RDONLY | O_NOCTTY | O_NONBLOCK | O_CLOEXEC);
        Check(p.guard >= 0, "open serial guard");
        Check(flock(p.guard, LOCK_EX | LOCK_NB) == 0, "lock serial device");
        Check(p.original.Save(p.guard), "save host termios2");
        p.saved = true;
        p.diagnostics.host_baud = p.options.baud_rate ? p.options.baud_rate : p.original.BaudRate();
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
        p.opened_at = Clock::now();
        p.stationary.clear();
        p.last_observed_tid = 0;
        p.configuration_uncertain = false;
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
            p.Pump(static_cast<int>(std::clamp<std::int64_t>(remaining, 0, p.options.rx_poll_wait_ms)));
            if (p.stopped) return Result::Failure(hw::MakeError(hw::ErrorCode::kNotRunning, "IMU stopped"));
            if (p.pending) {
                const auto f = *p.pending;
                const auto received = p.pending_receive;
                p.pending.reset();
                if (!detail::IsNewYesenseTid(f.tid, p.have_tid ? p.tid : 0)) {
                    ++p.diagnostics.rejected_tid;
                    continue;
                }
                const double norm2 = f.qw*f.qw + f.qx*f.qx + f.qy*f.qy + f.qz*f.qz;
                if (!std::isfinite(f.ax) || !std::isfinite(f.ay) || !std::isfinite(f.az) ||
                    !std::isfinite(f.wx) || !std::isfinite(f.wy) || !std::isfinite(f.wz) ||
                    ((f.fields & 4) && (!std::isfinite(norm2) || std::abs(norm2 - 1.0) > 0.001))) {
                    ++p.diagnostics.rejected_values;
                    continue;
                }
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
                sample.acceleration_valid = (f.fields & 1) != 0;
                sample.angular_velocity_valid = (f.fields & 2) != 0;
                sample.temperature_celsius = f.temperature;
                sample.temperature_valid = (f.fields & 8) != 0;
                sample.magnetic_field_tesla = {f.mx, f.my, f.mz};
                sample.magnetic_field_valid = (f.fields & 16) != 0;
                // Quaternion is retained for diagnostics, not declared R_reference_sensor
                // until the device convention is verified. No normalization/axis guessing.
                sample.orientation_valid = false;
                sample.orientation_present = (f.fields & 4) != 0;
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
namespace {
constexpr std::uint32_t kRates[] = {1,2,5,10,20,25,50,100,200,400};
constexpr std::uint32_t kBauds[] = {9600,38400,115200,460800,921600,19200,57600,76800,230400};
constexpr unsigned kOutputFields = 1|2|4|8|16|32|64;
unsigned ToMask(ImuOutputFields fields) {
    unsigned mask = 0;
    if (fields&1) mask |= 0x80;
    if (fields&2) mask |= 0x40;
    if (fields&4) mask |= 0x08;
    if (fields&8) mask |= 0x400;
    if (fields&16) mask |= 0x20;
    if (fields&32) mask |= 0x10;
    if (fields&64) mask |= 0x8000;
    return mask;
}
ImuOutputFields FromMask(unsigned mask) {
    if (mask & ~ToMask(kOutputFields)) throw std::runtime_error("device output mask includes fields outside the YIS321 profile");
    ImuOutputFields fields = 0;
    for (unsigned bit = 1; bit <= 64; bit <<= 1) if (mask & ToMask(bit)) fields |= bit;
    return fields;
}
unsigned FrameBytes(unsigned fields) {
    if (!fields) return 0;
    return 7 + ((fields&1)?14:0) + ((fields&2)?14:0) + ((fields&4)?18:0) +
        ((fields&8)?4:0) + ((fields&16)?28:0) + ((fields&32)?14:0) + ((fields&64)?12:0);
}
template<std::size_t N> unsigned Code(double value, const std::uint32_t (&table)[N]) {
    for (unsigned i=0; i<N; ++i) if (table[i] == value) return i+1;
    throw std::invalid_argument("value is unsupported by the YIS321 profile");
}
template<std::size_t N> std::uint32_t DecodeCode(unsigned code, const std::uint32_t (&table)[N]) {
    if (code == 0 || code > N) throw std::runtime_error("device code is outside the YIS321 documented profile");
    return table[code-1];
}
}

ImuCapabilities YesenseImu::Capabilities() const {
    ImuCapabilities c;
    c.configuration_access = impl_->options.device_control;
    c.output_rates_hz.assign(std::begin(kRates), std::end(kRates));
    c.baud_rates.assign(std::begin(kBauds), std::end(kBauds));
    // Native host setup supports all documented rates, including custom 76800 via LPIO.
    c.output_fields = kOutputFields;
    c.persistence = {ImuPersistence::kRam, ImuPersistence::kFlash};
    c.static_gyro_calibration = true;
    c.profile = "YIS321 documented 1..400 Hz profile; firmware-dependent higher codes are not assumed";
    return c;
}

hw::Result<ImuDeviceConfiguration> YesenseImu::ReadConfiguration() {
    auto& p = *impl_;
    std::lock_guard<std::mutex> lock(p.mutex);
    if (!p.options.device_control) return hw::Result<ImuDeviceConfiguration>::Failure(hw::MakeError(hw::ErrorCode::kNotSupported, "read-only session: enable device control explicitly"));
    try {
        p.EnsureControl();
        return hw::Result<ImuDeviceConfiguration>::Success(p.ReadCurrent());
    } catch (const std::exception& e) {
        return hw::Result<ImuDeviceConfiguration>::Failure(hw::MakeError(hw::ErrorCode::kProtocolError, e.what()));
    }
}

hw::Result<ImuConfigurationResult> YesenseImu::ApplyConfiguration(const ImuConfigurationPatch& patch) {
    using Result = hw::Result<ImuConfigurationResult>;
    auto& p = *impl_;
    std::lock_guard<std::mutex> lock(p.mutex);
    if (!p.options.device_control) return Result::Failure(hw::MakeError(hw::ErrorCode::kNotSupported, "read-only session: enable device control explicitly"));
    ImuConfigurationResult result;
    try {
        p.EnsureControl();
        if (!patch.output_rate_hz && !patch.output_fields && !patch.baud_rate) throw std::invalid_argument("empty configuration patch");
        if (patch.output_rate_hz) Code(*patch.output_rate_hz, kRates);
        if (patch.baud_rate) Code(*patch.baud_rate, kBauds);
        if (patch.output_fields && (*patch.output_fields & ~kOutputFields)) throw std::invalid_argument("unsupported output field");
        if (patch.persistence != ImuPersistence::kRam && patch.persistence != ImuPersistence::kFlash) throw std::invalid_argument("unsupported persistence");
        auto observed = p.ReadCurrent();
        const auto rate = patch.output_rate_hz.value_or(*observed.output_rate_hz);
        const auto fields = patch.output_fields.value_or(*observed.output_fields);
        const auto baud = patch.baud_rate.value_or(*observed.baud_rate);
        if (std::uint64_t(FrameBytes(fields))*rate*10 > std::uint64_t(baud)*8/10)
            throw std::invalid_argument("output rate/content exceeds 80% of UART bandwidth; increase baud or reduce output");
        result.observed = observed;
        const unsigned op = patch.persistence == ImuPersistence::kRam ? 1 : 2;
        const auto changed = [&](const std::string& setting, unsigned category, const std::vector<std::uint8_t>& payload) {
            ImuSettingResult item; item.setting = setting;
            try {
                const auto ack = p.Exchange(category, op, payload);
                if (ack.payload.size() != 1) throw std::runtime_error("invalid configuration ACK");
                if (ack.payload[0] == 0xff) {
                    item.state = ImuSettingState::kRejected; item.message = "device rejected setting; earlier settings remain applied";
                } else if (ack.payload[0] != 0) throw std::runtime_error("unknown configuration ACK status");
                else {
                    const auto reply = p.Exchange(category, 0, {});
                    if (reply.payload != payload) throw std::runtime_error("ACK received but readback differs");
                    item.state = ImuSettingState::kApplied;
                    item.message = "ACK and current readback match; persistence requested, flash durability not independently verified";
                    if (category == 3) observed.output_rate_hz = rate;
                    else observed.output_fields = fields;
                    observed.queried_at_monotonic_ns = hw::SteadyTimeNs();
                    result.observed = observed;
                }
            } catch (const std::exception& e) {
                p.configuration_uncertain = true; item.message = e.what(); result.observed.reset();
            }
            result.settings.push_back(item);
            return item.state == ImuSettingState::kApplied;
        };
        const auto change_baud = [&] {
            ImuSettingResult item; item.setting = "baud";
            try {
                // UART baud writes have no ACK. Keep the exclusive fd, switch host,
                // discard old-speed traffic, and query actual baud at the new speed.
                const auto command = detail::YesenseDecoder::Command(2, op, {static_cast<std::uint8_t>(Code(baud, kBauds))});
                const auto sent = p.serial->write(command);
                if (sent != command.size()) throw std::runtime_error("partial baud command; device state unknown");
                const auto drain_deadline = Clock::now() + std::chrono::milliseconds(100);
                int queued = 0;
                do {
                    Check(ioctl(p.guard, TIOCOUTQ, &queued) == 0, "query serial output queue");
                    if (p.stopped || Clock::now() >= drain_deadline) throw std::runtime_error("baud command drain deadline exceeded");
                    if (queued) std::this_thread::sleep_for(std::chrono::milliseconds(1));
                } while (queued);
                // Cover hardware FIFO shift time without an unbounded tcdrain call.
                std::this_thread::sleep_for(std::chrono::microseconds(command.size()*10000000ULL/p.diagnostics.host_baud + 1000));
                p.serial->setBaudRate(baud);
                p.options.baud_rate = baud; p.diagnostics.host_baud = baud;
                p.decoder = {}; p.replies.clear(); p.pending.reset(); p.stationary.clear();
                Check(tcflush(p.guard, TCIFLUSH) == 0, "discard old-speed input");
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                const auto reply = p.Exchange(2, 0, {}, 1000);
                if (reply.payload.size() != 1 || DecodeCode(reply.payload[0], kBauds) != baud)
                    throw std::runtime_error("baud change readback differs at new host speed");
                observed.baud_rate = baud; observed.host_baud_rate = baud;
                observed.queried_at_monotonic_ns = hw::SteadyTimeNs(); result.observed = observed;
                item.state = ImuSettingState::kApplied;
                item.message = "new host speed and device baud readback verified; no write ACK exists";
            } catch (const std::exception& e) {
                p.configuration_uncertain = true; p.stopped = true; result.observed.reset(); item.message = e.what();
                p.Error(hw::ErrorCode::kDisconnected, "baud change unconfirmed; explicitly reconnect with expected device baud");
            }
            result.settings.push_back(item); return item.state == ImuSettingState::kApplied;
        };
        // Keep intermediate bandwidth within the old/new link budgets.
        if (patch.baud_rate && baud > *observed.baud_rate && !change_baud()) return Result::Success(result);
        if (patch.output_rate_hz && rate <= *observed.output_rate_hz && !changed("rate", 3, {static_cast<std::uint8_t>(Code(rate,kRates))})) return Result::Success(result);
        if (patch.output_fields) {
            const unsigned mask = ToMask(fields);
            if (!changed("fields",4,{static_cast<std::uint8_t>(mask),static_cast<std::uint8_t>(mask>>8)})) return Result::Success(result);
        }
        if (patch.output_rate_hz && rate > *observed.output_rate_hz && !changed("rate",3,{static_cast<std::uint8_t>(Code(rate,kRates))})) return Result::Success(result);
        // A same-value Flash request must still be sent; RAM readback proves no persistence.
        if (patch.baud_rate && (baud <= *observed.baud_rate) &&
            (result.settings.empty() || result.settings.front().setting != "baud") && !change_baud()) return Result::Success(result);
        return Result::Success(result);
    } catch (const std::invalid_argument& e) {
        return Result::Failure(hw::MakeError(hw::ErrorCode::kInvalidConfig,e.what()));
    } catch (const std::exception& e) {
        return Result::Failure(hw::MakeError(hw::ErrorCode::kProtocolError,e.what()));
    }
}

hw::Result<ImuCalibrationResult> YesenseImu::CalibrateStaticGyro(const ImuCalibrationRequest& request) {
    auto& p = *impl_;
    std::lock_guard<std::mutex> lock(p.mutex);
    using Result = hw::Result<ImuCalibrationResult>;
    if (!p.options.device_control) return Result::Failure(hw::MakeError(hw::ErrorCode::kNotSupported,"read-only session"));
    bool issued = false;
    try {
        p.EnsureControl(); p.Pump(0);
        const auto now = Clock::now();
        if (now-p.opened_at < std::chrono::seconds(10)) throw std::runtime_error("wait at least 10 seconds after opening before static calibration");
        if (p.stationary.empty() || now-p.stationary.front().first < std::chrono::seconds(1) ||
            now-p.stationary.back().first > std::chrono::milliseconds(100)) throw std::runtime_error("need at least 1 second of fresh accel/gyro stationary observations");
        for (const auto& sample : p.stationary) if (now-sample.first <= std::chrono::seconds(1) && !sample.second)
            throw std::runtime_error("IMU is not stationary; calibration not sent");
        if (request.persistence != ImuPersistence::kRam && request.persistence != ImuPersistence::kFlash) throw std::runtime_error("unsupported persistence");
        issued = true;
        const auto reply = p.Exchange(0x4d, request.persistence == ImuPersistence::kRam ? 1 : 2, {0x50,1}, 2000);
        const auto status = reply.payload.back();
        if (reply.payload.size() == 2 && reply.payload[0] != 0x50) throw std::runtime_error("calibration response subtype mismatch");
        if (status == 0 || status == 0xa5) return Result::Success({ImuCalibrationState::kAccepted,"device accepted static gyro calibration; keep stationary at least 1 second; not a mounting/attitude calibration"});
        if (status == 0xff) return Result::Success({ImuCalibrationState::kRejected,"device rejected calibration"});
        p.configuration_uncertain = true;
        return Result::Success({ImuCalibrationState::kUnconfirmed,"unknown calibration status; do not retry automatically"});
    } catch (const std::exception& e) {
        if (issued) p.configuration_uncertain = true;
        if (p.configuration_uncertain) return Result::Success({ImuCalibrationState::kUnconfirmed,e.what()});
        return Result::Failure(hw::MakeError(hw::ErrorCode::kSafetyViolation,e.what()));
    }
}

ImuDeviceConfiguration YesenseImu::Impl::ReadCurrent() {
    ImuDeviceConfiguration c;
    const auto baud = Exchange(2,0,{});
    c.baud_rate = DecodeCode(baud.payload.at(0),kBauds);
    const auto rate = Exchange(3,0,{});
    c.output_rate_hz = DecodeCode(rate.payload.at(0),kRates);
    const auto fields = Exchange(4,0,{});
    if (fields.payload.size() != 2) throw std::runtime_error("invalid output fields readback");
    c.output_fields = FromMask(fields.payload[0] | (unsigned(fields.payload[1]) << 8));
    c.host_baud_rate = diagnostics.host_baud;
    c.queried_at_monotonic_ns = hw::SteadyTimeNs();
    return c;
}
} // namespace openember::sensor
