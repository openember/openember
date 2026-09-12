#include "endpoints/joint_controller_endpoint.hpp"

#include <algorithm>
#include <iostream>
#include <thread>
#include <utility>

#include "adapters/actuator_message_adapter.hpp"
#include "openember/hardware/timestamp.hpp"

namespace openember::services::hardware_interface {

namespace {

openember::actuator::MockJointControllerOptions BuildOptions(
    const EndpointConfig& config) {
    openember::actuator::MockJointControllerOptions options;
    options.controller_id = config.endpoint_id;
    options.state_rate_hz = config.publish_rate_hz;
    options.command_timeout_ms = config.command_timeout_ms;
    options.disabled_on_start = config.disabled_on_start;
    return options;
}

std::chrono::milliseconds ReadTimeoutForRate(double rate_hz) {
    const auto hz = std::max(rate_hz, 0.001);
    const auto period_ms = static_cast<int>(1000.0 / hz);
    return std::chrono::milliseconds(std::max(period_ms * 2, 10));
}

}  // namespace

JointControllerEndpoint::JointControllerEndpoint(EndpointConfig config,
                                                 EndpointContext context)
    : config_(std::move(config)),
      context_(std::move(context)),
      controller_(BuildOptions(config_)) {
    status_.endpoint_id = config_.endpoint_id;
    status_.device_id = config_.endpoint_id;
    status_.driver = config_.driver;
    status_.mode = config_.mode;
}

JointControllerEndpoint::~JointControllerEndpoint() {
    Stop();
}

const std::string& JointControllerEndpoint::Id() const {
    return config_.endpoint_id;
}

bool JointControllerEndpoint::Configure() {
    if (config_.command_topic.empty() || config_.state_topic.empty()) {
        SetError("joint controller endpoint requires command_topic and state_topic");
        return false;
    }

    std::lock_guard<std::mutex> lock(status_mutex_);
    status_.state = EndpointState::kConfigured;
    status_.message = "configured";
    return true;
}

bool JointControllerEndpoint::Start() {
    auto result = controller_.Start();
    if (!result.Ok()) {
        SetError(result.Err().message);
        return false;
    }

    state_publisher_ =
        context_.node->Advertise<openember::msgs::actuator::v1::JointState>(
            config_.state_topic);
    command_subscriber_ =
        context_.node->Subscribe<openember::msgs::actuator::v1::JointCommand>(
            config_.command_topic,
            [this](const openember::msgs::actuator::v1::JointCommand& msg) {
                HandleCommand(msg);
            });

    running_.store(true);
    thread_ = std::thread(&JointControllerEndpoint::Run, this);

    std::lock_guard<std::mutex> lock(status_mutex_);
    status_.state = EndpointState::kRunning;
    status_.message = "running";
    return true;
}

void JointControllerEndpoint::Stop() noexcept {
    running_.store(false);
    controller_.Disable();
    controller_.Stop();
    if (thread_.joinable()) {
        thread_.join();
    }

    std::lock_guard<std::mutex> lock(status_mutex_);
    if (status_.state != EndpointState::kError) {
        status_.state = EndpointState::kStopped;
        status_.message = "stopped";
    }
}

EndpointStatus JointControllerEndpoint::Status() const {
    std::lock_guard<std::mutex> lock(status_mutex_);
    return status_;
}

void JointControllerEndpoint::Run() {
    const auto read_timeout = ReadTimeoutForRate(config_.publish_rate_hz);

    while (running_.load() && openember::Ok()) {
        auto step = controller_.Step();
        if (!step.Ok()) {
            RecordError(step.Err().message);
            std::this_thread::sleep_for(read_timeout);
            continue;
        }

        auto result = controller_.ReadState(read_timeout);
        if (!result.Ok()) {
            if (result.Err().code == openember::hardware::ErrorCode::kTimeout) {
                RecordTimeout(result.Err().message);
            } else {
                RecordError(result.Err().message);
            }
            continue;
        }

        MessageHeaderContext header;
        header.source_node = context_.source_node;
        header.source_instance = context_.source_instance;
        header.robot_id = context_.robot_id;
        header.timestamp_unix_ns = openember::hardware::UnixTimeNs();

        auto msg = ToProto(result.Value(), header);
        const bool published = state_publisher_.Publish(msg);
        if (!published) {
            RecordError("failed to publish joint state");
        }

        UpdateStatusFromController(result.Value(), published);
    }
}

void JointControllerEndpoint::HandleCommand(
    const openember::msgs::actuator::v1::JointCommand& msg) {
    auto command = ToDomain(msg);
    if (command.command_time_monotonic_ns == 0) {
        command.command_time_monotonic_ns = openember::hardware::SteadyTimeNs();
    }

    auto result = controller_.SetCommand(command);
    std::lock_guard<std::mutex> lock(status_mutex_);
    status_.last_command_monotonic_ns = command.command_time_monotonic_ns;
    if (result.Ok()) {
        status_.command_count += 1;
        status_.message = "command accepted";
        return;
    }

    status_.command_count += 1;
    status_.error_count += 1;
    status_.last_error = result.Err().message;
    status_.message = result.Err().message;
}

void JointControllerEndpoint::RecordError(const std::string& message) {
    std::lock_guard<std::mutex> lock(status_mutex_);
    status_.error_count += 1;
    status_.last_error = message;
    status_.message = message;
}

void JointControllerEndpoint::RecordTimeout(const std::string& message) {
    std::lock_guard<std::mutex> lock(status_mutex_);
    status_.timeout_count += 1;
    status_.last_error = message;
    status_.message = message;
}

void JointControllerEndpoint::SetError(const std::string& message) {
    std::lock_guard<std::mutex> lock(status_mutex_);
    status_.state = EndpointState::kError;
    status_.error_count += 1;
    status_.last_error = message;
    status_.message = message;
}

void JointControllerEndpoint::UpdateStatusFromController(
    const openember::actuator::JointState& state,
    bool published) {
    const auto controller_status = controller_.Status();

    std::lock_guard<std::mutex> lock(status_mutex_);
    status_.sequence = state.sequence;
    status_.sample_count += 1;
    if (published) {
        status_.publish_count += 1;
    }
    status_.error_count = std::max(status_.error_count, controller_status.error_count);
    status_.timeout_count =
        std::max(status_.timeout_count, controller_status.timeout_count);
    status_.last_sample_monotonic_ns = state.sample_time_monotonic_ns;
    status_.last_publish_monotonic_ns = openember::hardware::SteadyTimeNs();
    status_.last_command_monotonic_ns =
        controller_status.last_command_monotonic_ns;
    status_.message = state.message;
    if (!controller_status.last_error.message.empty()) {
        status_.last_error = controller_status.last_error.message;
    }
}

}  // namespace openember::services::hardware_interface
