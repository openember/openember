#include "endpoints/gnss_endpoint.hpp"

#include <iostream>
#include <utility>

#include "adapters/sensor_message_adapter.hpp"
#include "openember/hardware/timestamp.hpp"

namespace openember::services::hardware_interface {

GnssEndpoint::GnssEndpoint(EndpointConfig config, EndpointContext context)
    : config_(std::move(config)),
      context_(std::move(context)),
      gnss_(openember::sensor::MockGnssOptions{
          config_.endpoint_id,
          config_.frame_id,
          config_.publish_rate_hz}) {
    status_.endpoint_id = config_.endpoint_id;
    status_.device_id = config_.endpoint_id;
    status_.driver = config_.driver;
    status_.mode = config_.mode;
}

GnssEndpoint::~GnssEndpoint() {
    Stop();
}

const std::string& GnssEndpoint::Id() const {
    return config_.endpoint_id;
}

bool GnssEndpoint::Configure() {
    std::lock_guard<std::mutex> lock(status_mutex_);
    status_.state = EndpointState::kConfigured;
    status_.message = "configured";
    return true;
}

bool GnssEndpoint::Start() {
    auto result = gnss_.Start();
    if (!result.Ok()) {
        SetError(result.Err().message);
        return false;
    }

    publisher_ =
        context_.node->Advertise<openember::msgs::sensor::v1::GnssFix>(
            config_.topic);
    running_.store(true);
    thread_ = std::thread(&GnssEndpoint::Run, this);

    std::lock_guard<std::mutex> lock(status_mutex_);
    status_.state = EndpointState::kRunning;
    status_.message = "running";
    return true;
}

void GnssEndpoint::Stop() noexcept {
    running_.store(false);
    gnss_.Stop();
    if (thread_.joinable()) {
        thread_.join();
    }

    std::lock_guard<std::mutex> lock(status_mutex_);
    if (status_.state != EndpointState::kError) {
        status_.state = EndpointState::kStopped;
        status_.message = "stopped";
    }
}

EndpointStatus GnssEndpoint::Status() const {
    std::lock_guard<std::mutex> lock(status_mutex_);
    return status_;
}

void GnssEndpoint::Run() {
    while (running_.load() && openember::Ok()) {
        auto result = gnss_.ReadFix(std::chrono::milliseconds(1200));
        if (!result.Ok()) {
            RecordError(result.Err().message);
            continue;
        }

        MessageHeaderContext header;
        header.source_node = context_.source_node;
        header.source_instance = context_.source_instance;
        header.robot_id = context_.robot_id;
        header.timestamp_unix_ns = openember::hardware::UnixTimeNs();

        auto msg = ToProto(result.Value(), header);
        if (!publisher_.Publish(msg)) {
            RecordError("failed to publish GNSS fix");
            continue;
        }

        std::lock_guard<std::mutex> lock(status_mutex_);
        status_.sequence = msg.sequence();
        status_.sample_count += 1;
        status_.publish_count += 1;
        status_.last_sample_monotonic_ns = msg.sample_time_monotonic_ns();
        status_.last_publish_monotonic_ns = openember::hardware::SteadyTimeNs();
    }
}

void GnssEndpoint::RecordError(const std::string& message) {
    std::lock_guard<std::mutex> lock(status_mutex_);
    status_.error_count += 1;
    status_.last_error = message;
    status_.message = message;
}

void GnssEndpoint::SetError(const std::string& message) {
    std::lock_guard<std::mutex> lock(status_mutex_);
    status_.state = EndpointState::kError;
    status_.error_count += 1;
    status_.last_error = message;
    status_.message = message;
}

}  // namespace openember::services::hardware_interface
