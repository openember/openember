#include "openember/sensor/imu_driver.hpp"
#include "openember/sensor/imu_driver_yaml.hpp"
#include "openember/hardware/timestamp.hpp"
#include "sample_json.hpp"
#include "imu_commands.hpp"
#include <atomic>
#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <csignal>
#include <cstdio>
#include <deque>
#include <fcntl.h>
#include <iostream>
#include <mutex>
#include <optional>
#include <poll.h>
#include <sstream>
#include <stdexcept>
#include <thread>
#include <unistd.h>

namespace {
namespace sensor = openember::sensor;
namespace hw = openember::hardware;
using Clock = std::chrono::steady_clock;
volatile std::sig_atomic_t interrupted = 0, terminating = 0;
void Signal(int signal) { if (signal == SIGTERM) terminating = 1; interrupted = 1; }
double Number(const std::string& text, double minimum, double maximum) {
    std::size_t end = 0;
    const double value = std::stod(text, &end);
    if (end != text.size() || !std::isfinite(value) || value < minimum || value > maximum)
        throw std::invalid_argument("number outside allowed range: " + text);
    return value;
}
std::uint32_t Integer(const std::string& text, std::uint32_t minimum, std::uint32_t maximum) {
    const double value = Number(text, minimum, maximum);
    if (value != std::floor(value)) throw std::invalid_argument("expected integer: " + text);
    return static_cast<std::uint32_t>(value);
}
const char* Health(hw::HealthState value) {
    switch (value) {
    case hw::HealthState::kOk: return "OK";
    case hw::HealthState::kWarning: return "WARNING";
    case hw::HealthState::kError: return "ERROR";
    case hw::HealthState::kStale: return "STALE";
    default: return "UNKNOWN";
    }
}
const char* Availability(hw::DeviceAvailability value) {
    switch (value) {
    case hw::DeviceAvailability::kOffline: return "OFFLINE";
    case hw::DeviceAvailability::kOnline: return "ONLINE";
    case hw::DeviceAvailability::kDegraded: return "DEGRADED";
    case hw::DeviceAvailability::kError: return "ERROR";
    default: return "UNKNOWN";
    }
}
struct Snapshot {
    std::uint64_t samples = 0, timeouts = 0, receive = 0;
    double elapsed = 0, max_interval_ms = 0, interval_sum_ms = 0;
    std::uint64_t intervals = 0;
    std::string error;
    std::optional<sensor::ImuSample> latest;
};
class Session {
public:
    explicit Session(sensor::ImuDriverConfig config) : config_(std::move(config)), registry_(sensor::BuiltinImuDrivers()) { device_ = registry_.Create(config_); }
    ~Session() { Close(); }
    void Open() {
        Close();
        device_ = registry_.Create(config_);
        const auto result = device_->Start();
        if (!result) throw std::runtime_error(result.Err().message);
        { std::lock_guard<std::mutex> lock(mutex_); snapshot_ = {}; queue_.clear(); recording_ = overflow_ = false; }
        start_ = Clock::now(); running_ = true;
        try { worker_ = std::thread([this] { Acquire(); }); }
        catch (...) { running_ = false; device_->Stop(); throw; }
        std::cout << "OPEN sensor=" << config_.sensor_id << " driver=" << config_.driver << '\n';
    }
    void Close() noexcept {
        running_ = false;
        if (worker_.joinable()) worker_.join();
        if (device_) device_->Stop();
    }
    bool Running() const { return running_; }
    Snapshot Get() const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto snapshot = snapshot_;
        snapshot.elapsed = start_.time_since_epoch().count() ? std::chrono::duration<double>(Clock::now() - start_).count() : 0;
        return snapshot;
    }
    sensor::ImuSample Next(std::uint64_t after, double seconds) {
        std::unique_lock<std::mutex> lock(mutex_);
        const auto deadline = Clock::now() + std::chrono::duration<double>(seconds);
        while (snapshot_.samples <= after && running_ && !interrupted && !terminating && Clock::now() < deadline)
            condition_.wait_for(lock, std::chrono::milliseconds(20));
        if (interrupted || terminating) throw std::runtime_error("sample interrupted");
        if (snapshot_.samples <= after || !snapshot_.latest)
            throw std::runtime_error(snapshot_.error.empty() ? "no new sample before deadline (is device open?)" : snapshot_.error);
        return *snapshot_.latest;
    }
    void Status() const {
        const auto s = Get();
        std::cout << "STATUS running=" << std::boolalpha << Running() << " samples=" << s.samples
            << " read_timeouts=" << s.timeouts << " acquisition_error=" << sensor_cli::Quote(s.error);
        const auto now = hw::SteadyTimeNs();
        if (s.latest && s.receive && now >= s.receive)
            std::cout << " receive_age_ms=" << double(now - s.receive) / 1e6;
        else std::cout << " receive_age_ms=unknown";
        std::cout << '\n';
        if (device_) {
            const auto status = device_->Status();
            std::cout << "DEVICE health=" << Health(status.health)
                << " availability=" << Availability(status.availability)
                << " message=" << sensor_cli::Quote(status.message)
                << " error=" << hw::ToString(status.last_error.code) << '\n';
        }
    }
    void Info() const {
        if (!device_) throw std::runtime_error("open device first");
        const auto& info = device_->Info();
        std::cout << "INFO sensor=" << info.sensor_id << " driver=" << info.driver << " model=" << sensor_cli::Quote(info.model)
            << " vendor=" << sensor_cli::Quote(info.vendor) << " frame_id=" << info.frame_id << '\n';
    }
    void Config() const {
        std::cout << "CONFIG driver=" << config_.driver << " sensor=" << config_.sensor_id << " frame_id=" << config_.frame_id
            << " device=" << sensor_cli::Quote(config_.device) << " baud=" << config_.baud_rate
            << " rx_poll_wait_ms=" << config_.rx_poll_wait_ms << " mock_rate_hz=" << config_.mock_rate_hz
            << " device_control=" << config_.device_control << "\n";
    }
    void Diagnostics() const {
        const auto* diagnostics = dynamic_cast<const sensor::ISensorDiagnostics*>(device_.get());
        std::cout << "DRIVER_DIAGNOSTICS supported=" << (diagnostics ? "true" : "false");
        if (diagnostics) for (const auto& c : diagnostics->ReadDiagnostics()) std::cout << ' ' << c.name << '=' << c.value;
        std::cout << '\n';
    }
    sensor::IImuConfiguration& Configurable() const {
        auto* control = dynamic_cast<sensor::IImuConfiguration*>(device_.get());
        if (!control) throw std::runtime_error("driver has no IMU configuration capability");
        return *control;
    }
    void Capabilities() const {
        const auto c = Configurable().Capabilities();
        std::cout << "CAPABILITIES configuration_access=" << c.configuration_access
            << " profile=" << sensor_cli::Quote(c.profile) << " rates=";
        for (auto rate : c.output_rates_hz) std::cout << rate << ',';
        std::cout << " baud_rates=";
        for (auto baud : c.baud_rates) std::cout << baud << ',';
        std::cout << " fields=" << sensor_cli::Fields(c.output_fields) << " persistence=";
        for (auto storage : c.persistence) std::cout << (storage == sensor::ImuPersistence::kRam ? "ram," : "flash,");
        std::cout << " static_gyro_calibration=" << c.static_gyro_calibration << '\n';
    }
    void DeviceConfiguration() {
        const auto result = Configurable().ReadConfiguration();
        if (!result) throw std::runtime_error(result.Err().message);
        sensor_cli::PrintConfiguration(result.Value());
    }
    void Apply(const sensor::ImuConfigurationPatch& patch) {
        const auto result = Configurable().ApplyConfiguration(patch);
        if (!result) throw std::runtime_error(result.Err().message);
        bool complete = !result.Value().settings.empty();
        for (const auto& setting : result.Value().settings) {
            std::cout << "SETTING name=" << setting.setting << " state=" << sensor_cli::SettingState(setting.state)
                << " storage=" << (patch.persistence == sensor::ImuPersistence::kRam ? "ram" : "flash")
                << " message=" << sensor_cli::Quote(setting.message) << '\n';
            complete &= setting.state == sensor::ImuSettingState::kApplied;
        }
        if (result.Value().observed) {
            sensor_cli::PrintConfiguration(*result.Value().observed);
            if (result.Value().observed->host_baud_rate) config_.baud_rate = result.Value().observed->host_baud_rate;
        }
        if (!complete) throw std::runtime_error("configuration incomplete; inspect per-field results, do not automatically retry writes");
    }
    void Calibrate(sensor::ImuPersistence storage) {
        auto* calibration = dynamic_cast<sensor::IImuCalibration*>(device_.get());
        if (!calibration) throw std::runtime_error("driver does not support static gyro calibration");
        const auto result = calibration->CalibrateStaticGyro({storage});
        if (!result) throw std::runtime_error(result.Err().message);
        using State = sensor::ImuCalibrationState;
        const auto state = result.Value().state;
        std::cout << "CALIBRATION state=" << (state == State::kAccepted ? "ACCEPTED" : state == State::kCompleted ? "COMPLETED" : state == State::kRejected ? "REJECTED" : "UNCONFIRMED")
            << " message=" << sensor_cli::Quote(result.Value().message) << '\n';
        if (state == State::kRejected || state == State::kUnconfirmed) throw std::runtime_error("calibration not confirmed");
    }
    void HostBaud(std::uint32_t baud) {
        if (Running()) throw std::runtime_error("close before changing host-only connection settings");
        if (config_.driver == "mock") throw std::runtime_error("Mock has no UART baud");
        auto next = config_; next.baud_rate = baud;
        registry_.Create(next); config_ = next;
        std::cout << "HOST_BAUD configured=" << baud << " device_unchanged=true; use open to reconnect\n";
    }
    void StartRecord() {
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.clear(); overflow_ = false; recording_ = true;
    }
    std::deque<sensor::ImuSample> DrainRecord(bool finish = false) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (finish) recording_ = false;
        if (overflow_) throw std::runtime_error("record queue overflow: file is incomplete; acquisition continues");
        std::deque<sensor::ImuSample> queue;
        queue.swap(queue_); return queue;
    }
    void StopRecord() {
        std::lock_guard<std::mutex> lock(mutex_);
        recording_ = false; queue_.clear();
    }
private:
    void Acquire() noexcept {
        try {
            while (running_) {
                auto result = device_->ReadSample(std::chrono::milliseconds(50));
                std::lock_guard<std::mutex> lock(mutex_);
                if (!running_) break;
                if (!result) {
                    if (result.Err().code == hw::ErrorCode::kTimeout) { ++snapshot_.timeouts; continue; }
                    snapshot_.error = result.Err().message; break;
                }
                const auto& s = result.Value();
                if (snapshot_.latest && snapshot_.receive && snapshot_.latest->clock_domain == s.clock_domain && s.receive_time_monotonic_ns >= snapshot_.receive) {
                    const double interval = double(s.receive_time_monotonic_ns - snapshot_.receive) / 1e6;
                    snapshot_.max_interval_ms = std::max(snapshot_.max_interval_ms, interval);
                    snapshot_.interval_sum_ms += interval; ++snapshot_.intervals;
                }
                snapshot_.latest = s; snapshot_.receive = s.receive_time_monotonic_ns; ++snapshot_.samples;
                if (recording_) {
                    if (queue_.size() >= 4096) { overflow_ = true; recording_ = false; }
                    else queue_.push_back(s);
                }
                condition_.notify_all();
            }
        } catch (const std::exception& e) {
            std::lock_guard<std::mutex> lock(mutex_); snapshot_.error = e.what();
        } catch (...) {
            std::lock_guard<std::mutex> lock(mutex_); snapshot_.error = "unexpected acquisition failure";
        }
        running_ = false; condition_.notify_all();
    }
    sensor::ImuDriverConfig config_;
    sensor::ImuDriverRegistry registry_;
    std::unique_ptr<sensor::IImu> device_;
    std::atomic_bool running_{false};
    std::thread worker_;
    mutable std::mutex mutex_;
    std::condition_variable condition_;
    Snapshot snapshot_;
    Clock::time_point start_{};
    bool recording_ = false, overflow_ = false;
    std::deque<sensor::ImuSample> queue_;
};
void Help() {
    std::cout << "Commands:\n"
        "  help | drivers | info | status | config | diagnostics\n"
        "  capabilities | device-config\n"
        "  set rate HZ [ram|flash] | set fields accel,gyro,quaternion [ram|flash]\n"
        "  set baud BPS [ram|flash] | host-baud BPS (closed session only)\n"
        "  calibrate static-gyro [ram|flash] (keep still; real device: 10s after open)\n"
        "  open | close | reopen\n"
        "  sample [--timeout SECONDS]\n"
        "  stream [--hz 10] [--seconds SECONDS]\n"
        "  stats [--seconds 10]\n"
        "  record --file PATH [--seconds SECONDS]\n"
        "  quit | exit\n"
        "Ctrl+C ends stream/stats/record and returns to prompt; at prompt it exits.\n"
        "Display Hz never changes device rate. Fields carry validity; quaternion is raw.\n";
}
void Command(Session& session, const std::vector<std::string>& args) {
    if (args.empty()) return;
    const auto& command = args[0];
    if (command == "set" || command == "calibrate") {
        const std::size_t base = command == "set" ? 3 : 2;
        if (args.size() != base && args.size() != base+1) throw std::invalid_argument("invalid setting/calibration arguments; see help");
        auto storage = sensor::ImuPersistence::kRam;
        if (args.size() == base+1) {
            if (args.back() == "flash") storage = sensor::ImuPersistence::kFlash;
            else if (args.back() != "ram") throw std::invalid_argument("storage must be ram or flash");
        }
        if (command == "calibrate") {
            if (args[1] != "static-gyro") throw std::invalid_argument("unsupported calibration type");
            session.Calibrate(storage); return;
        }
        sensor::ImuConfigurationPatch patch; patch.persistence = storage;
        if (args[1] == "rate") patch.output_rate_hz = Number(args[2],.01,10000);
        else if (args[1] == "baud") patch.baud_rate = Integer(args[2],1,4000000);
        else if (args[1] == "fields") patch.output_fields = sensor_cli::ParseFields(args[2]);
        else throw std::invalid_argument("unknown setting");
        session.Apply(patch); return;
    }
    if (command == "host-baud") {
        if (args.size() != 2) throw std::invalid_argument("host-baud BPS");
        session.HostBaud(Integer(args[1],1,4000000)); return;
    }
    if (command == "sample" || command == "stream" || command == "stats" || command == "record") {
        double seconds = command == "stats" ? 10 : command == "sample" ? 1 : 0;
        double hz = 10;
        std::string path;
        for (std::size_t i = 1; i < args.size(); ++i) {
            const auto& key = args[i];
            if (++i == args.size()) throw std::invalid_argument("missing option value");
            if (key == "--seconds" && command != "sample") seconds = Number(args[i], .001, 86400);
            else if (key == "--timeout" && command == "sample") seconds = Number(args[i], .001, 60);
            else if (key == "--hz" && command == "stream") hz = Number(args[i], .1, 100);
            else if (key == "--file" && command == "record") path = args[i];
            else throw std::invalid_argument("unknown command option: " + key);
        }
        if (!session.Running()) throw std::runtime_error("device not running; use open/reopen");
        if (command == "sample") {
            std::cout << sensor_cli::SampleJson(session.Next(session.Get().samples, seconds)) << '\n'; return;
        }
        FILE* file = nullptr;
        if (command == "record") {
            if (path.empty()) throw std::invalid_argument("record requires --file PATH");
            const int fd = ::open(path.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0644);
            if (fd < 0) throw std::runtime_error("cannot create recording (existing files are not overwritten): " + path);
            file = fdopen(fd, "w");
            if (!file) { ::close(fd); throw std::runtime_error("cannot open recording stream"); }
            session.StartRecord();
        }
        const auto initial = session.Get();
        auto bucket = initial;
        const auto begin = Clock::now();
        auto next_display = begin;
        auto next_report = begin + std::chrono::seconds(1);
        std::uint64_t displayed = 0, written = 0;
        auto write = [&](bool finish) {
            for (const auto& s : session.DrainRecord(finish)) {
                const auto text = sensor_cli::SampleJson(s) + '\n';
                if (fwrite(text.data(), 1, text.size(), file) != text.size()) throw std::runtime_error("record write failed");
                ++written;
            }
        };
        try {
            while (!interrupted && !terminating) {
                const auto now = Clock::now();
                const double elapsed = std::chrono::duration<double>(now - begin).count();
                if (seconds > 0 && elapsed >= seconds) break;
                if (!session.Running()) throw std::runtime_error("acquisition stopped: " + session.Get().error);
                const auto s = session.Get();
                if (command == "record") write(false);
                if (command == "stream" && now >= next_display) {
                    if (s.latest && s.samples != displayed) { std::cout << sensor_cli::SampleJson(*s.latest) << std::endl; displayed = s.samples; }
                    next_display = now + std::chrono::duration_cast<Clock::duration>(std::chrono::duration<double>(1 / hz));
                }
                if (command == "stats" && now >= next_report) {
                    std::cout << "STATS window_hz=" << (s.samples - bucket.samples) / (s.elapsed - bucket.elapsed)
                        << " samples=" << s.samples - initial.samples << " read_timeouts=" << s.timeouts - initial.timeouts << std::endl;
                    bucket = s; next_report = now + std::chrono::seconds(1);
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            }
            if (file) {
                write(true);
                if (fflush(file) != 0) throw std::runtime_error("record flush failed");
                const int result = fclose(file); file = nullptr;
                if (result != 0) throw std::runtime_error("record close failed");
                std::cout << "RECORD samples=" << written << " file=" << sensor_cli::Quote(path) << '\n';
            }
        } catch (...) { session.StopRecord(); if (file) fclose(file); throw; }
        if (command == "stats") {
            const auto s = session.Get();
            const auto intervals = s.intervals - initial.intervals;
            std::cout << "STATS_SUMMARY samples=" << s.samples - initial.samples
                << " delivered_hz=" << (s.samples - initial.samples) / std::max(.000001, s.elapsed - initial.elapsed)
                << " read_timeouts=" << s.timeouts - initial.timeouts
                << " mean_receive_interval_ms=" << (intervals ? (s.interval_sum_ms - initial.interval_sum_ms) / intervals : 0)
                << " session_max_receive_interval_ms=" << s.max_interval_ms << '\n';
            session.Diagnostics();
        }
        return;
    }
    if (args.size() != 1) throw std::invalid_argument("unexpected command arguments");
    if (command == "help" || command == "?") Help();
    else if (command == "drivers") { for (const auto& driver : sensor::BuiltinImuDrivers().Drivers()) std::cout << driver << '\n'; }
    else if (command == "info") session.Info();
    else if (command == "capabilities") session.Capabilities();
    else if (command == "device-config") session.DeviceConfiguration();
    else if (command == "status") session.Status();
    else if (command == "config") session.Config();
    else if (command == "diagnostics") session.Diagnostics();
    else if (command == "open" || command == "reopen") session.Open();
    else if (command == "close") { session.Close(); std::cout << "CLOSED\n"; }
    else throw std::invalid_argument("unknown command: " + command);
}
std::vector<std::string> Split(const std::string& line) {
    std::istringstream in(line); std::vector<std::string> args; std::string word;
    while (in >> std::quoted(word)) args.push_back(word);
    if (!in.eof()) throw std::invalid_argument("invalid quoted command");
    return args;
}
std::optional<std::string> Line() {
    std::string line;
    while (!interrupted && !terminating) {
        pollfd input{STDIN_FILENO, POLLIN, 0};
        const int ready = poll(&input, 1, 100);
        if (ready < 0) { if (errno == EINTR) continue; throw std::runtime_error("stdin poll failed"); }
        if (!ready) continue;
        char ch;
        const auto count = read(STDIN_FILENO, &ch, 1);
        if (count == 0) return line.empty() ? std::nullopt : std::optional<std::string>(line);
        if (count < 0) { if (errno == EINTR) continue; throw std::runtime_error("stdin read failed"); }
        if (ch == '\n') return line;
        if (line.size() >= 8192) throw std::runtime_error("command exceeds 8192 bytes");
        line.push_back(ch);
    }
    return std::nullopt;
}
} // namespace
int main(int argc, char** argv) {
    try {
        std::cout << std::boolalpha << std::setprecision(8);
        if (argc == 1 || (argc == 2 && std::string(argv[1]) == "--help")) {
            std::cout << "openember_sensor_cli imu (--driver mock|yesense [--device PATH --baud 921600]\n"
                "  [--rate HZ] [--frame-id ID] [--sensor ID] | --config device.yaml --sensor ID)\n"
                "  [--closed] [--device-control] [--command \"COMMAND\" ...]\n";
            Help(); return 0;
        }
        if (argc == 3 && std::string(argv[1]) == "imu" && std::string(argv[2]) == "--help") { Help(); return 0; }
        if (std::string(argv[1]) != "imu") throw std::invalid_argument("expected imu subcommand");
        sensor::ImuDriverConfig config;
        std::string yaml, sensor_id;
        bool direct = false, selected = false, closed = false;
        bool device_control = false;
        bool rate_set = false, baud_set = false, frame_set = false;
        std::vector<std::string> commands;
        for (int i = 2; i < argc; ++i) {
            const std::string key = argv[i];
            if (key == "--closed") { closed = true; continue; }
            if (key == "--device-control") { device_control = true; continue; }
            if (++i == argc) throw std::invalid_argument("missing option value: " + key);
            const std::string value = argv[i];
            if (key == "--config") yaml = value;
            else if (key == "--sensor") sensor_id = value;
            else if (key == "--command") commands.push_back(value);
            else {
                direct = true;
                if (key == "--driver") { config.driver = value; selected = true; }
                else if (key == "--device") config.device = value;
                else if (key == "--baud") { config.baud_rate = Integer(value, 1, 4000000); baud_set = true; }
                else if (key == "--rate") { config.mock_rate_hz = Number(value, 1, 4000); rate_set = true; }
                else if (key == "--frame-id") { config.frame_id = value; frame_set = true; }
                else throw std::invalid_argument("unknown option: " + key);
            }
        }
        if (!yaml.empty()) {
            if (device_control) throw std::invalid_argument("device.yaml readonly profile cannot enable writes; use explicit --driver/--device with --device-control for bench configuration");
            if (direct || sensor_id.empty()) throw std::invalid_argument("--config requires --sensor; direct connection options cannot override YAML");
            config = sensor::LoadImuDriverConfig(yaml, sensor_id);
        } else {
            if (!selected) throw std::invalid_argument("explicit --driver or --config required");
            if (!sensor_id.empty()) config.sensor_id = sensor_id;
            if (rate_set && config.driver != "mock") throw std::invalid_argument("--rate is only for Mock; device output rate is not configured by this tool");
            if (baud_set && config.driver == "mock") throw std::invalid_argument("Mock has no host UART baud");
            if (config.driver == "yesense" && !frame_set) config.frame_id = "yesense_sensor_unverified";
        }
        config.device_control = device_control;
        // Validate assembly before touching a device, including in --closed mode.
        sensor::BuiltinImuDrivers().Create(config);
        struct sigaction action{}; action.sa_handler = Signal; sigemptyset(&action.sa_mask);
        sigaction(SIGINT, &action, nullptr); sigaction(SIGTERM, &action, nullptr);
        Session session(config);
        session.Config();
        if (!closed) session.Open();
        int exit_code = 0;
        auto execute = [&](const std::string& line) {
            const auto args = Split(line);
            if (!args.empty() && (args[0] == "quit" || args[0] == "exit")) {
                if (args.size() != 1) throw std::invalid_argument("unexpected quit arguments");
                return false;
            }
            Command(session, args); interrupted = 0; return true;
        };
        if (!commands.empty()) {
            for (const auto& command : commands) {
                if (terminating || !execute(command)) break;
            }
        } else {
            std::cout << "SENSOR_CLI_READY type=imu; help for commands" << std::endl;
            while (!terminating) {
                if (isatty(STDIN_FILENO)) std::cout << "imu> " << std::flush;
                const auto line = Line(); if (!line) break;
                try { if (!execute(*line)) break; }
                catch (const std::exception& e) { std::cerr << "ERROR " << e.what() << std::endl; interrupted = 0; exit_code = 1; }
                std::cout << std::flush;
            }
        }
        session.Close();
        return terminating ? 128 + SIGTERM : exit_code;
    } catch (const std::exception& e) { std::cerr << "ERROR " << e.what() << '\n'; return 1; }
}
