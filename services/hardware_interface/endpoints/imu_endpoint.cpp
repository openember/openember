#include "endpoints/imu_endpoint.hpp"

#include <iostream>
#include <stdexcept>
#include <utility>

#include "adapters/sensor_message_adapter.hpp"
#include "openember/hardware/timestamp.hpp"
#include "openember/sensor/mock_imu.hpp"
#include "openember/services/hardware_interface/imu_endpoint_factory.hpp"

namespace openember::services::hardware_interface {

ImuEndpoint::ImuEndpoint(EndpointConfig config, EndpointContext context)
    : ImuEndpoint(config, std::move(context), std::make_unique<openember::sensor::MockImu>(
          openember::sensor::MockImuOptions{config.endpoint_id, config.frame_id, config.publish_rate_hz})) {}

ImuEndpoint::ImuEndpoint(EndpointConfig config, EndpointContext context,
                         std::unique_ptr<openember::sensor::IImu> driver)
    : config_(std::move(config)),
      context_(std::move(context)),
      imu_(std::move(driver)) {
    if (!imu_) throw std::invalid_argument("IMU driver must not be null");
    status_.endpoint_id = config_.endpoint_id;
    status_.device_id = config_.endpoint_id;
    status_.driver = config_.driver;
    status_.mode = config_.mode;
}

ImuEndpoint::~ImuEndpoint() {
    Stop();
}

const std::string& ImuEndpoint::Id() const {
    return config_.endpoint_id;
}

bool ImuEndpoint::Configure() {
    if (config_.topic.empty() || !config_.command_topic.empty() ||
        config_.endpoint_id != imu_->Info().sensor_id || config_.frame_id != imu_->Info().frame_id) {
        SetError("IMU endpoint requires a sample-only topic and matching driver sensor/frame IDs");
        return false;
    }
    std::lock_guard<std::mutex> lock(status_mutex_);
    status_.state = EndpointState::kConfigured;
    status_.message = "configured";
    return true;
}

bool ImuEndpoint::Start() {
    if (running_.load()) return true;
    if (thread_.joinable()) return false; // Owner must Stop before restarting.
    try {
        if (!context_.node) throw std::runtime_error("IMU endpoint requires a node");
        auto result = imu_->Start();
        if (!result.Ok()) throw std::runtime_error(result.Err().message);
        publisher_ = context_.node->Advertise<openember::msgs::sensor::v1::ImuSample>(config_.topic);
        const auto device_status = imu_->Status();
        {
            std::lock_guard<std::mutex> lock(status_mutex_);
            status_.state = EndpointState::kRunning;
            status_.degraded = device_status.health != openember::hardware::HealthState::kOk;
            status_.message = device_status.message;
        }
        running_.store(true);
        thread_ = std::thread(&ImuEndpoint::Run, this);
        return true;
    } catch (const std::exception& e) {
        running_.store(false);
        imu_->Stop();
        SetError(e.what());
        return false;
    } catch (...) {
        running_.store(false);
        imu_->Stop();
        SetError("unexpected IMU start exception");
        return false;
    }
}

void ImuEndpoint::Stop() noexcept {
    running_.store(false);
    imu_->Stop();
    if (thread_.joinable()) {
        thread_.join();
    }

    std::lock_guard<std::mutex> lock(status_mutex_);
    if (status_.state != EndpointState::kError) {
        status_.state = EndpointState::kStopped;
        status_.message = "stopped";
    }
}

EndpointStatus ImuEndpoint::Status() const {
    std::lock_guard<std::mutex> lock(status_mutex_);
    return status_;
}

void ImuEndpoint::Run() {
    try {
        while (running_.load() && openember::Ok()) {
            auto result = imu_->ReadSample(std::chrono::milliseconds(100));
            if (!running_.load()) break;
            if (!result.Ok()) {
                if (result.Err().code != openember::hardware::ErrorCode::kTimeout) {
                    SetError(result.Err().message);
                    break;
                }
                RecordError(result.Err().message, true);
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                continue;
            }

            MessageHeaderContext header;
            header.source_node = context_.source_node;
            header.source_instance = context_.source_instance;
            header.robot_id = context_.robot_id;
            header.timestamp_unix_ns = openember::hardware::UnixTimeNs();

            auto msg = ToProto(result.Value(), header);
            if (!publisher_.Publish(msg)) {
                RecordError("failed to publish IMU sample");
                continue;
            }

            const auto device_status = imu_->Status();
            std::lock_guard<std::mutex> lock(status_mutex_);
            status_.degraded = device_status.health != openember::hardware::HealthState::kOk;
            status_.message = device_status.message;
            status_.sequence = msg.sequence();
            status_.sample_count += 1;
            status_.publish_count += 1;
            status_.last_sample_monotonic_ns = msg.receive_time_monotonic_ns() != 0
                ? msg.receive_time_monotonic_ns() : msg.sample_time_monotonic_ns();
            status_.last_publish_monotonic_ns = openember::hardware::SteadyTimeNs();
        }
    } catch (const std::exception& e) {
        SetError(e.what());
    } catch (...) {
        SetError("unexpected IMU worker exception");
    }
    running_.store(false);
}

void ImuEndpoint::RecordError(const std::string& message, bool timeout) {
    std::lock_guard<std::mutex> lock(status_mutex_);
    status_.error_count += 1;
    if (timeout) status_.timeout_count += 1;
    status_.degraded = true;
    status_.last_error = message;
    status_.message = message;
}

void ImuEndpoint::SetError(const std::string& message) {
    std::lock_guard<std::mutex> lock(status_mutex_);
    status_.state = EndpointState::kError;
    status_.error_count += 1;
    status_.last_error = message;
    status_.message = message;
}

std::unique_ptr<HardwareEndpoint> MakeImuEndpoint(
    EndpointConfig config, EndpointContext context,
    std::unique_ptr<sensor::IImu> driver) {
    return std::make_unique<ImuEndpoint>(std::move(config), std::move(context), std::move(driver));
}

}  // namespace openember::services::hardware_interface
