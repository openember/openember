#include "openember/services/hardware_interface/imu_endpoint_factory.hpp"
#include "openember/hardware/timestamp.hpp"
#include "openember/init.hpp"
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <atomic>
#include <condition_variable>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <thread>

namespace {
namespace hi = openember::services::hardware_interface;
namespace hw = openember::hardware;
namespace sensor = openember::sensor;
void Check(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
struct Trace { std::atomic_int starts{0}, stops{0}, reads{0}; };
class Driver final : public sensor::IImu {
public:
    Driver(std::string which, std::shared_ptr<Trace> trace) : which_(std::move(which)), trace_(std::move(trace)) {
        info_.sensor_id = "imu0"; info_.frame_id = "sensor";
    }
    const sensor::SensorInfo& Info() const override { return info_; }
    hw::DeviceStatus Status() const override {
        hw::DeviceStatus status;
        status.health = hw::HealthState::kWarning;
        status.message = "unverified reference";
        return status;
    }
    hw::Result<void> Start() override {
        ++trace_->starts;
        if (which_ == "failure") return hw::Result<void>::Failure(hw::MakeError(hw::ErrorCode::kOpenFailed, "open failed"));
        return hw::Result<void>::Success();
    }
    void Stop() noexcept override {
        { std::lock_guard<std::mutex> lock(mutex_); stopped_ = true; }
        ++trace_->stops;
        cv_.notify_all();
    }
    hw::Result<sensor::ImuSample> ReadSample(std::chrono::milliseconds timeout) override {
        ++trace_->reads;
        if (which_ == "exception") throw std::runtime_error("read exception");
        std::unique_lock<std::mutex> lock(mutex_);
        cv_.wait_for(lock, which_ == "cancel" ? std::chrono::seconds(5) : timeout, [&] { return stopped_; });
        if (stopped_) return hw::Result<sensor::ImuSample>::Failure(hw::MakeError(hw::ErrorCode::kNotRunning, "stopped"));
        if (which_ == "timeout") return hw::Result<sensor::ImuSample>::Failure(hw::MakeError(hw::ErrorCode::kTimeout, "timeout"));
        sensor::ImuSample sample;
        sample.sensor_id = "imu0"; sample.frame_id = "sensor";
        sample.sequence = trace_->reads;
        sample.receive_time_monotonic_ns = hw::SteadyTimeNs();
        return hw::Result<sensor::ImuSample>::Success(sample);
    }
private:
    std::string which_;
    std::shared_ptr<Trace> trace_;
    sensor::SensorInfo info_;
    std::mutex mutex_;
    std::condition_variable cv_;
    bool stopped_ = false;
};
std::string Listen() {
    const int fd = socket(AF_INET, SOCK_STREAM, 0);
    Check(fd >= 0, "socket failed");
    sockaddr_in address{};
    address.sin_family = AF_INET; address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    socklen_t size = sizeof(address);
    const bool ok = bind(fd, reinterpret_cast<sockaddr*>(&address), size) == 0 &&
        getsockname(fd, reinterpret_cast<sockaddr*>(&address), &size) == 0;
    close(fd);
    Check(ok, "port allocation failed");
    return "tcp/127.0.0.1:" + std::to_string(ntohs(address.sin_port));
}
}
int main(int argc, char** argv) {
    try {
        Check(argc == 2, "case required");
        const std::string which = argv[1];
        auto trace = std::make_shared<Trace>();
        hi::EndpointConfig config;
        config.endpoint_id = "imu0"; config.frame_id = "sensor"; config.topic = "/test/imu";
        if (which == "config") {
            bool rejected = false;
            try { hi::MakeImuEndpoint(config, {}, nullptr); } catch (const std::invalid_argument&) { rejected = true; }
            Check(rejected, "null driver accepted");
            config.command_topic = "/unsafe";
            auto endpoint = hi::MakeImuEndpoint(config, {}, std::make_unique<Driver>(which, trace));
            Check(!endpoint->Configure() && trace->starts == 0, "command topic accepted");
            config.command_topic.clear(); config.frame_id = "wrong";
            endpoint = hi::MakeImuEndpoint(config, {}, std::make_unique<Driver>(which, trace));
            Check(!endpoint->Configure(), "mismatched frame accepted");
            return 0;
        }
        openember::RuntimeOptions options;
        options.link.profile = openember::link::Profile::kRouter; options.link.listen = Listen();
        openember::Init(options);
        {
            hi::EndpointContext context;
            context.node = openember::CreateNode("imu_endpoint_test");
            auto endpoint = hi::MakeImuEndpoint(config, context, std::make_unique<Driver>(which, trace));
            Check(endpoint->Configure(), "configure failed");
            const bool started = endpoint->Start();
            if (which == "failure") {
                Check(!started && trace->stops > 0 && endpoint->Status().state == hi::EndpointState::kError, "partial start not rolled back");
            } else {
                Check(started, "Start failed");
                const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
                while (std::chrono::steady_clock::now() < deadline) {
                    const auto status = endpoint->Status();
                    if ((which == "publish" && status.publish_count) || (which == "timeout" && status.timeout_count) ||
                        (which == "exception" && status.state == hi::EndpointState::kError) || (which == "cancel" && trace->reads)) break;
                    std::this_thread::sleep_for(std::chrono::milliseconds(5));
                }
                const auto status = endpoint->Status();
                if (which == "publish") Check(status.publish_count > 0 && status.degraded && status.last_sample_monotonic_ns > 0, "driver warning/receive timestamp lost");
                else if (which == "timeout") Check(status.timeout_count > 0 && status.publish_count == 0, "timeout published cached sample");
                else if (which == "exception") Check(status.state == hi::EndpointState::kError, "worker exception not contained");
                else Check(which == "cancel" && trace->reads > 0, "unknown case or reader not started");
                const auto begin = std::chrono::steady_clock::now();
                endpoint->Stop(); endpoint->Stop();
                Check(std::chrono::steady_clock::now() - begin < std::chrono::milliseconds(500), "Stop did not cancel driver");
            }
        }
        openember::Shutdown();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n'; openember::Shutdown(); return 1;
    }
}
