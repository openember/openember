#include "openember/services/hardware_interface/hardware_interface_app.hpp"

#include <iostream>
#include <thread>
#include <utility>
#include <unistd.h>

#include "endpoints/gnss_endpoint.hpp"
#include "endpoints/imu_endpoint.hpp"
#if OPENEMBER_HARDWARE_ENDPOINT_JOINT_CONTROLLER
#include "endpoints/joint_controller_endpoint.hpp"
#endif
#include "endpoints/temperature_endpoint.hpp"
#include "openember/hardware/timestamp.hpp"
#include "openember/init.hpp"
#include "openember/msgs/common/v1/common.pb.h"
#include "openember/msgs/device/v1/device.pb.h"
#include "openember/msgs/lifecycle/v1/lifecycle.pb.h"

namespace openember::services::hardware_interface {

namespace {

constexpr const char* kNodeInfoTopic = "/nodes/hardware_interface/info";
constexpr const char* kHeartbeatTopic = "/nodes/hardware_interface/heartbeat";
constexpr const char* kDiagnosticsTopic = "/diagnostics/hardware_interface";
constexpr auto kHeartbeatPeriod = std::chrono::seconds(1);
constexpr std::uint64_t kNodeInfoPublishInterval = 10;

void FillHeader(openember::msgs::common::v1::Header* header,
                const HardwareInterfaceAppOptions& options,
                std::uint64_t sequence) {
    header->set_source_node(options.node_name);
    header->set_source_instance(options.instance_id);
    header->set_robot_id(options.robot_id);
    header->set_sequence(sequence);
    header->set_timestamp_unix_ns(openember::hardware::UnixTimeNs());
}

void ConfigureQos(openember::msgs::common::v1::QosProfile* qos) {
    qos->set_queue_size(8);
    qos->set_reliability(openember::msgs::common::v1::RELIABILITY_RELIABLE);
    qos->set_durability(openember::msgs::common::v1::DURABILITY_VOLATILE);
}

void AddTopic(openember::msgs::node::v1::NodeInfo* info,
              const std::string& name,
              const std::string& type,
              openember::msgs::node::v1::EndpointDirection direction) {
    auto* topic = info->add_topics();
    topic->set_name(name);
    topic->set_type(type);
    topic->set_direction(direction);
    ConfigureQos(topic->mutable_qos());
}

void AddLabel(openember::msgs::node::v1::NodeInfo* info,
              const std::string& key,
              const std::string& value) {
    auto* label = info->add_labels();
    label->set_key(key);
    label->set_value(value);
}

openember::msgs::diagnostics::v1::DiagnosticLevel ToDiagnosticLevel(
    const EndpointStatus& status) {
    if (status.state == EndpointState::kError) {
        return openember::msgs::diagnostics::v1::DIAGNOSTIC_LEVEL_ERROR;
    }
    if (status.error_count > 0 || status.timeout_count > 0) {
        return openember::msgs::diagnostics::v1::DIAGNOSTIC_LEVEL_WARNING;
    }
    if (status.state == EndpointState::kRunning) {
        return openember::msgs::diagnostics::v1::DIAGNOSTIC_LEVEL_OK;
    }
    return openember::msgs::diagnostics::v1::DIAGNOSTIC_LEVEL_STALE;
}

void AddMetric(openember::msgs::common::v1::Metric* metric,
               const std::string& name,
               double value,
               const std::string& unit) {
    metric->set_name(name);
    metric->set_value(value);
    metric->set_unit(unit);
}

std::string DeviceInfoTopic(const std::string& endpoint_id) {
    return "/devices/" + endpoint_id + "/info";
}

std::string DeviceStateTopic(const std::string& endpoint_id) {
    return "/devices/" + endpoint_id + "/state";
}

std::string MessageTypeNameForEndpoint(const std::string& endpoint_type) {
    if (endpoint_type == "imu") {
        return "openember.msgs.sensor.v1.ImuSample";
    }
    if (endpoint_type == "temperature") {
        return "openember.msgs.sensor.v1.TemperatureSample";
    }
    if (endpoint_type == "gnss") {
        return "openember.msgs.sensor.v1.GnssFix";
    }
    if (endpoint_type == "joint_controller") {
        return "openember.msgs.actuator.v1.JointState";
    }
    return "openember.msgs.sensor.v1.SensorStatus";
}

std::string CommandTypeNameForEndpoint(const std::string& endpoint_type) {
    if (endpoint_type == "joint_controller") {
        return "openember.msgs.actuator.v1.JointCommand";
    }
    return "";
}

openember::msgs::device::v1::DeviceCategory ToDeviceCategory(
    const EndpointConfig& config) {
    if (config.type == "imu" || config.type == "temperature" || config.type == "gnss") {
        return openember::msgs::device::v1::DEVICE_CATEGORY_SENSOR;
    }
    if (config.type == "joint_controller") {
        return openember::msgs::device::v1::DEVICE_CATEGORY_ACTUATOR;
    }
    return openember::msgs::device::v1::DEVICE_CATEGORY_UNSPECIFIED;
}

openember::msgs::device::v1::DeviceAvailability ToDeviceAvailability(
    EndpointState state) {
    using Availability = openember::msgs::device::v1::DeviceAvailability;
    switch (state) {
    case EndpointState::kCreated:
    case EndpointState::kConfigured:
        return Availability::DEVICE_AVAILABILITY_OFFLINE;
    case EndpointState::kRunning:
        return Availability::DEVICE_AVAILABILITY_ONLINE;
    case EndpointState::kStopping:
    case EndpointState::kStopped:
        return Availability::DEVICE_AVAILABILITY_OFFLINE;
    case EndpointState::kError:
        return Availability::DEVICE_AVAILABILITY_ERROR;
    }
    return Availability::DEVICE_AVAILABILITY_UNSPECIFIED;
}

openember::msgs::common::v1::HealthState ToHealthState(const EndpointStatus& status) {
    if (status.state == EndpointState::kError) {
        return openember::msgs::common::v1::HEALTH_STATE_ERROR;
    }
    if (status.error_count > 0) {
        return openember::msgs::common::v1::HEALTH_STATE_WARNING;
    }
    if (status.state == EndpointState::kRunning) {
        return openember::msgs::common::v1::HEALTH_STATE_OK;
    }
    return openember::msgs::common::v1::HEALTH_STATE_STALE;
}

}  // namespace

HardwareInterfaceApp::HardwareInterfaceApp(std::shared_ptr<openember::Node> node,
                                           HardwareInterfaceConfig config,
                                           HardwareInterfaceAppOptions options)
    : node_(std::move(node)),
      config_(std::move(config)),
      options_(std::move(options)),
      start_time_(std::chrono::steady_clock::now()),
      start_time_unix_ns_(openember::hardware::UnixTimeNs()) {}

HardwareInterfaceApp::~HardwareInterfaceApp() {
    Stop();
}

bool HardwareInterfaceApp::Start() {
    node_info_pub_ =
        node_->Advertise<openember::msgs::node::v1::NodeInfo>(kNodeInfoTopic);
    heartbeat_pub_ =
        node_->Advertise<openember::msgs::node::v1::NodeHeartbeat>(kHeartbeatTopic);
    diagnostics_pub_ =
        node_->Advertise<openember::msgs::diagnostics::v1::DiagnosticArray>(
            kDiagnosticsTopic);

    if (!BuildEndpoints()) {
        return false;
    }
    for (const auto& endpoint : endpoints_) {
        const auto* config = FindEndpointConfig(endpoint->Id());
        const bool critical = config && config->critical;
        if (!endpoint->Configure()) {
            std::cerr << "hardware_interface: configure failed for "
                      << endpoint->Id() << std::endl;
            if (critical) {
                return false;
            }
            continue;
        }
        if (!endpoint->Start()) {
            std::cerr << "hardware_interface: start failed for "
                      << endpoint->Id() << std::endl;
            if (critical) {
                return false;
            }
            continue;
        }
        std::cout << "hardware_interface started endpoint "
                  << endpoint->Id() << std::endl;
    }

    for (const auto& endpoint : endpoints_) {
        const auto* config = FindEndpointConfig(endpoint->Id());
        if (!config) {
            continue;
        }
        device_info_pubs_[endpoint->Id()] =
            node_->Advertise<openember::msgs::device::v1::DeviceInfo>(
                DeviceInfoTopic(endpoint->Id()));
        device_state_pubs_[endpoint->Id()] =
            node_->Advertise<openember::msgs::device::v1::DeviceState>(
                DeviceStateTopic(endpoint->Id()));
    }

    return true;
}

void HardwareInterfaceApp::Stop() noexcept {
    for (auto it = endpoints_.rbegin(); it != endpoints_.rend(); ++it) {
        (*it)->Stop();
    }
}

void HardwareInterfaceApp::Spin() {
    std::uint64_t sequence = 0;
    std::uint64_t info_sequence = 0;

    while (openember::Ok()) {
        if (sequence % kNodeInfoPublishInterval == 0) {
            (void)node_info_pub_.Publish(BuildNodeInfo(info_sequence));
            ++info_sequence;
        }

        (void)heartbeat_pub_.Publish(BuildHeartbeat(sequence));
        (void)diagnostics_pub_.Publish(BuildDiagnostics(sequence));

        for (const auto& endpoint : endpoints_) {
            const auto status = endpoint->Status();
            const auto* config = FindEndpointConfig(endpoint->Id());
            if (!config) {
                continue;
            }
            auto info_it = device_info_pubs_.find(endpoint->Id());
            auto state_it = device_state_pubs_.find(endpoint->Id());
            if (info_it != device_info_pubs_.end()) {
                (void)info_it->second.Publish(
                    BuildDeviceInfo(*config, status, sequence));
            }
            if (state_it != device_state_pubs_.end()) {
                (void)state_it->second.Publish(
                    BuildDeviceState(*config, status, sequence));
            }
        }

        ++sequence;
        std::this_thread::sleep_for(kHeartbeatPeriod);
    }
}

bool HardwareInterfaceApp::BuildEndpoints() {
    EndpointContext context;
    context.node = node_;
    context.source_node = options_.node_name;
    context.source_instance = options_.instance_id;
    context.robot_id = options_.robot_id;

    std::size_t enabled_count = 0;
    for (const auto& endpoint_config : config_.endpoints) {
        if (!endpoint_config.enabled) {
            continue;
        }
        ++enabled_count;

        const auto previous_size = endpoints_.size();
        if (endpoint_config.type == "imu") {
#if OPENEMBER_HARDWARE_ENDPOINT_IMU
            endpoints_.push_back(std::make_unique<ImuEndpoint>(endpoint_config, context));
#endif
        } else if (endpoint_config.type == "temperature") {
#if OPENEMBER_HARDWARE_ENDPOINT_TEMPERATURE
            endpoints_.push_back(
                std::make_unique<TemperatureEndpoint>(endpoint_config, context));
#endif
        } else if (endpoint_config.type == "gnss") {
#if OPENEMBER_HARDWARE_ENDPOINT_GNSS
            endpoints_.push_back(std::make_unique<GnssEndpoint>(endpoint_config, context));
#endif
        } else if (endpoint_config.type == "joint_controller") {
#if OPENEMBER_HARDWARE_ENDPOINT_JOINT_CONTROLLER
            endpoints_.push_back(
                std::make_unique<JointControllerEndpoint>(endpoint_config, context));
#endif
        } else {
            std::cerr << "hardware_interface: unknown endpoint type "
                      << endpoint_config.type << std::endl;
        }

        if (endpoints_.size() == previous_size) {
            std::cerr << "hardware_interface: endpoint " << endpoint_config.endpoint_id
                      << " is unavailable in this build" << std::endl;
            if (endpoint_config.critical) {
                return false;
            }
        }
    }

    if (enabled_count > 0 && endpoints_.empty()) {
        std::cerr << "hardware_interface: no enabled endpoint is available"
                  << std::endl;
        return false;
    }
    return true;
}

const EndpointConfig* HardwareInterfaceApp::FindEndpointConfig(
    const std::string& endpoint_id) const {
    for (const auto& config : config_.endpoints) {
        if (config.endpoint_id == endpoint_id) {
            return &config;
        }
    }
    return nullptr;
}

openember::msgs::node::v1::NodeInfo HardwareInterfaceApp::BuildNodeInfo(
    std::uint64_t sequence) const {
    openember::msgs::node::v1::NodeInfo info;
    FillHeader(info.mutable_header(), options_, sequence);
    info.set_node_name(options_.node_name);
    info.set_instance_id(options_.instance_id);
    info.set_process_name("openember_hardware_interface");
    info.set_process_id(static_cast<std::uint32_t>(getpid()));
    info.set_kind(openember::msgs::node::v1::NODE_KIND_SERVICE);
    info.set_version("0.1.0");
    info.set_start_time_unix_ns(start_time_unix_ns_);

    AddTopic(&info,
             kNodeInfoTopic,
             "openember.msgs.node.v1.NodeInfo",
             openember::msgs::node::v1::ENDPOINT_DIRECTION_PUBLISHER);
    AddTopic(&info,
             kHeartbeatTopic,
             "openember.msgs.node.v1.NodeHeartbeat",
             openember::msgs::node::v1::ENDPOINT_DIRECTION_PUBLISHER);
    AddTopic(&info,
             kDiagnosticsTopic,
             "openember.msgs.diagnostics.v1.DiagnosticArray",
             openember::msgs::node::v1::ENDPOINT_DIRECTION_PUBLISHER);

    for (const auto& endpoint : endpoints_) {
        const auto status = endpoint->Status();
        const auto* config = FindEndpointConfig(status.endpoint_id);
        if (!config) {
            continue;
        }
        AddTopic(&info,
                 config->topic,
                 MessageTypeNameForEndpoint(config->type),
                 openember::msgs::node::v1::ENDPOINT_DIRECTION_PUBLISHER);
        if (!config->command_topic.empty()) {
            AddTopic(&info,
                     config->command_topic,
                     CommandTypeNameForEndpoint(config->type),
                     openember::msgs::node::v1::ENDPOINT_DIRECTION_SUBSCRIBER);
        }
        AddTopic(&info,
                 DeviceInfoTopic(config->endpoint_id),
                 "openember.msgs.device.v1.DeviceInfo",
                 openember::msgs::node::v1::ENDPOINT_DIRECTION_PUBLISHER);
        AddTopic(&info,
                 DeviceStateTopic(config->endpoint_id),
                 "openember.msgs.device.v1.DeviceState",
                 openember::msgs::node::v1::ENDPOINT_DIRECTION_PUBLISHER);
    }

    AddLabel(&info, "role", "service");
    info.add_capabilities("hardware_interface");
    info.add_capabilities("mock_hardware_endpoints");
    return info;
}

openember::msgs::node::v1::NodeHeartbeat HardwareInterfaceApp::BuildHeartbeat(
    std::uint64_t sequence) const {
    openember::msgs::node::v1::NodeHeartbeat heartbeat;
    FillHeader(heartbeat.mutable_header(), options_, sequence);
    heartbeat.set_node_name(options_.node_name);
    heartbeat.set_instance_id(options_.instance_id);
    heartbeat.set_lifecycle_state(
        openember::msgs::lifecycle::v1::LIFECYCLE_STATE_ACTIVE);

    std::uint64_t error_count = 0;
    bool any_error = false;
    bool any_warning = false;
    for (const auto& endpoint : endpoints_) {
        const auto status = endpoint->Status();
        error_count += status.error_count;
        any_error = any_error || status.state == EndpointState::kError;
        any_warning = any_warning || status.error_count > 0 || status.timeout_count > 0;
    }

    heartbeat.set_health(
        any_error ? openember::msgs::common::v1::HEALTH_STATE_ERROR
                  : (any_warning ? openember::msgs::common::v1::HEALTH_STATE_WARNING
                                 : openember::msgs::common::v1::HEALTH_STATE_OK));
    heartbeat.set_uptime_ms(static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start_time_).count()));
    heartbeat.set_error_count(static_cast<std::uint32_t>(error_count));
    heartbeat.set_status_message("hardware interface running");

    AddMetric(heartbeat.add_metrics(),
              "hardware_interface.endpoints",
              static_cast<double>(endpoints_.size()),
              "count");
    AddMetric(heartbeat.add_metrics(),
              "hardware_interface.errors",
              static_cast<double>(error_count),
              "count");
    return heartbeat;
}

openember::msgs::diagnostics::v1::DiagnosticArray HardwareInterfaceApp::BuildDiagnostics(
    std::uint64_t sequence) const {
    openember::msgs::diagnostics::v1::DiagnosticArray array;
    FillHeader(array.mutable_header(), options_, sequence);

    for (const auto& endpoint : endpoints_) {
        const auto status = endpoint->Status();
        auto* diagnostic = array.add_status();
        FillHeader(diagnostic->mutable_header(), options_, status.sequence);
        diagnostic->set_name("hardware_interface." + status.endpoint_id);
        diagnostic->set_node_name(options_.node_name);
        diagnostic->set_hardware_id(status.device_id);
        diagnostic->set_level(ToDiagnosticLevel(status));
        diagnostic->set_message(status.message);

        auto* state_value = diagnostic->add_values();
        state_value->set_key("state");
        state_value->set_value(ToString(status.state));

        auto* driver_value = diagnostic->add_values();
        driver_value->set_key("driver");
        driver_value->set_value(status.driver);

        auto* mode_value = diagnostic->add_values();
        mode_value->set_key("mode");
        mode_value->set_value(status.mode);

        AddMetric(diagnostic->add_metrics(),
                  "sample_count",
                  static_cast<double>(status.sample_count),
                  "count");
        AddMetric(diagnostic->add_metrics(),
                  "publish_count",
                  static_cast<double>(status.publish_count),
                  "count");
        AddMetric(diagnostic->add_metrics(),
                  "error_count",
                  static_cast<double>(status.error_count),
                  "count");
        AddMetric(diagnostic->add_metrics(),
                  "command_count",
                  static_cast<double>(status.command_count),
                  "count");
        AddMetric(diagnostic->add_metrics(),
                  "timeout_count",
                  static_cast<double>(status.timeout_count),
                  "count");
    }

    return array;
}

openember::msgs::device::v1::DeviceInfo HardwareInterfaceApp::BuildDeviceInfo(
    const EndpointConfig& config,
    const EndpointStatus& status,
    std::uint64_t sequence) const {
    openember::msgs::device::v1::DeviceInfo info;
    FillHeader(info.mutable_header(), options_, sequence);
    info.set_device_id(config.endpoint_id);
    info.set_name(config.endpoint_id);
    info.set_category(ToDeviceCategory(config));
    info.set_driver(config.driver);
    info.set_path(config.mode + "://" + config.endpoint_id);
    info.set_bus(config.mode);
    info.add_capabilities("hardware_endpoint");
    info.add_capabilities(config.type);

    auto* endpoint_type = info.add_labels();
    endpoint_type->set_key("endpoint_type");
    endpoint_type->set_value(config.type);

    auto* endpoint_state = info.add_labels();
    endpoint_state->set_key("endpoint_state");
    endpoint_state->set_value(ToString(status.state));

    auto* mode = info.add_labels();
    mode->set_key("mode");
    mode->set_value(config.mode);

    return info;
}

openember::msgs::device::v1::DeviceState HardwareInterfaceApp::BuildDeviceState(
    const EndpointConfig& config,
    const EndpointStatus& status,
    std::uint64_t sequence) const {
    openember::msgs::device::v1::DeviceState state;
    FillHeader(state.mutable_header(), options_, sequence);
    state.set_device_id(config.endpoint_id);
    state.set_availability(ToDeviceAvailability(status.state));
    state.set_health(ToHealthState(status));
    state.set_message(status.message);

    AddMetric(state.add_metrics(),
              "sample_count",
              static_cast<double>(status.sample_count),
              "count");
    AddMetric(state.add_metrics(),
              "publish_count",
              static_cast<double>(status.publish_count),
              "count");
    AddMetric(state.add_metrics(),
              "error_count",
              static_cast<double>(status.error_count),
              "count");
    AddMetric(state.add_metrics(),
              "command_count",
              static_cast<double>(status.command_count),
              "count");
    AddMetric(state.add_metrics(),
              "timeout_count",
              static_cast<double>(status.timeout_count),
              "count");

    auto* endpoint_type = state.add_properties();
    endpoint_type->set_key("endpoint_type");
    endpoint_type->set_value(config.type);

    auto* endpoint_state = state.add_properties();
    endpoint_state->set_key("endpoint_state");
    endpoint_state->set_value(ToString(status.state));

    if (!status.last_error.empty()) {
        auto* last_error = state.add_properties();
        last_error->set_key("last_error");
        last_error->set_value(status.last_error);
    }

    return state;
}

}  // namespace openember::services::hardware_interface
