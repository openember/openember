/*
 * Copyright (c) 2022-2026, OpenEmber Team
 * SPDX-License-Identifier: Apache-2.0
 */

#include <unistd.h>

#include <chrono>
#include <cstdint>
#include <exception>
#include <string>
#include <thread>
#include <vector>

#define MODULE_NAME "device_manager"
#define LOG_TAG MODULE_NAME
#include "openember.h"

#include "openember/framework/system_bus.hpp"
#include "openember/init.hpp"
#include "openember/msgs/common/v1/common.pb.h"
#include "openember/msgs/device/v1/device.pb.h"
#include "openember/msgs/lifecycle/v1/lifecycle.pb.h"
#include "openember/msgs/node/v1/node.pb.h"
#include "openember/node.hpp"

namespace {

constexpr const char* kRobotId = "openember";
constexpr const char* kInstanceId = MODULE_NAME;
constexpr const char* kNodeInfoTopic = "/nodes/device_manager/info";
constexpr const char* kHeartbeatTopic = "/nodes/device_manager/heartbeat";
constexpr const char* kDeviceInfoTopic = "/devices/runtime_demo/info";
constexpr const char* kDeviceStateTopic = "/devices/runtime_demo/state";
constexpr const char* kDeviceQueryService = "/devices/query";
constexpr auto kHeartbeatPeriod = std::chrono::seconds(1);
constexpr std::uint64_t kNodeInfoPublishInterval = 10;

std::uint64_t UnixTimeNs() {
    const auto now = std::chrono::system_clock::now().time_since_epoch();
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(now).count());
}

void FillHeader(openember::msgs::common::v1::Header* header,
                std::uint64_t sequence,
                const openember::msgs::common::v1::Header* request_header = nullptr) {
    header->set_source_node(MODULE_NAME);
    header->set_source_instance(kInstanceId);
    header->set_robot_id(kRobotId);
    header->set_sequence(sequence);
    header->set_timestamp_unix_ns(UnixTimeNs());
    if (request_header) {
        header->set_trace_id(request_header->trace_id());
        header->set_request_id(request_header->request_id());
    }
}

void FillStatus(openember::msgs::common::v1::Status* status,
                bool ok,
                const std::string& message) {
    status->set_code(ok ? 0 : 1);
    status->set_message(message);
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

void AddService(openember::msgs::node::v1::NodeInfo* info,
                const std::string& name,
                const std::string& request_type,
                const std::string& response_type) {
    auto* service = info->add_services();
    service->set_name(name);
    service->set_request_type(request_type);
    service->set_response_type(response_type);
    service->set_direction(openember::msgs::node::v1::ENDPOINT_DIRECTION_SERVICE_SERVER);
    ConfigureQos(service->mutable_qos());
}

void AddLabel(openember::msgs::node::v1::NodeInfo* info,
              const std::string& key,
              const std::string& value) {
    auto* label = info->add_labels();
    label->set_key(key);
    label->set_value(value);
}

struct ManagedDevice {
    openember::msgs::device::v1::DeviceInfo info;
    openember::msgs::device::v1::DeviceState state;
};

openember::msgs::node::v1::NodeInfo BuildNodeInfo(std::uint64_t sequence,
                                                  std::uint64_t start_time_unix_ns) {
    openember::msgs::node::v1::NodeInfo info;
    FillHeader(info.mutable_header(), sequence);
    info.set_node_name(MODULE_NAME);
    info.set_instance_id(kInstanceId);
    info.set_process_name(MODULE_NAME);
    info.set_process_id(static_cast<std::uint32_t>(getpid()));
    info.set_kind(openember::msgs::node::v1::NODE_KIND_SYSTEM);
    info.set_version("0.1.0");
    info.set_start_time_unix_ns(start_time_unix_ns);

    AddTopic(&info,
             kNodeInfoTopic,
             "openember.msgs.node.v1.NodeInfo",
             openember::msgs::node::v1::ENDPOINT_DIRECTION_PUBLISHER);
    AddTopic(&info,
             kHeartbeatTopic,
             "openember.msgs.node.v1.NodeHeartbeat",
             openember::msgs::node::v1::ENDPOINT_DIRECTION_PUBLISHER);
    AddTopic(&info,
             kDeviceInfoTopic,
             "openember.msgs.device.v1.DeviceInfo",
             openember::msgs::node::v1::ENDPOINT_DIRECTION_PUBLISHER);
    AddTopic(&info,
             kDeviceStateTopic,
             "openember.msgs.device.v1.DeviceState",
             openember::msgs::node::v1::ENDPOINT_DIRECTION_PUBLISHER);
    AddService(&info,
               kDeviceQueryService,
               "openember.msgs.device.v1.DeviceQuery",
               "openember.msgs.device.v1.DeviceQueryResponse");

    AddLabel(&info, "role", "system");
    info.add_capabilities("device_registry");
    info.add_capabilities("device_query");
    return info;
}

std::vector<ManagedDevice> BuildInitialDevices() {
    ManagedDevice runtime;

    auto* info_header = runtime.info.mutable_header();
    FillHeader(info_header, 0);
    runtime.info.set_device_id("runtime_demo");
    runtime.info.set_name("Runtime Demo Device");
    runtime.info.set_category(openember::msgs::device::v1::DEVICE_CATEGORY_SENSOR);
    runtime.info.set_driver("openember.demo");
    runtime.info.set_path("virtual://runtime_demo");
    runtime.info.set_bus("virtual");
    runtime.info.add_capabilities("heartbeat_source");
    runtime.info.add_capabilities("demo_sensor");
    auto* label = runtime.info.add_labels();
    label->set_key("source");
    label->set_value("static");

    FillHeader(runtime.state.mutable_header(), 0);
    runtime.state.set_device_id(runtime.info.device_id());
    runtime.state.set_availability(
        openember::msgs::device::v1::DEVICE_AVAILABILITY_ONLINE);
    runtime.state.set_health(openember::msgs::common::v1::HEALTH_STATE_OK);
    runtime.state.set_message("runtime demo device online");

    return {runtime};
}

openember::msgs::device::v1::DeviceQueryResponse BuildDeviceQueryResponse(
    const openember::msgs::device::v1::DeviceQuery& request,
    const std::vector<ManagedDevice>& devices,
    std::uint64_t sequence) {
    openember::msgs::device::v1::DeviceQueryResponse response;
    FillHeader(response.mutable_header(), sequence, &request.header());
    FillStatus(response.mutable_status(), true, "ok");

    for (const auto& device : devices) {
        if (!request.device_id().empty() &&
            request.device_id() != device.info.device_id()) {
            continue;
        }
        if (request.category() !=
                openember::msgs::device::v1::DEVICE_CATEGORY_UNSPECIFIED &&
            request.category() != device.info.category()) {
            continue;
        }

        *response.add_devices() = device.info;
        if (request.include_state()) {
            *response.add_states() = device.state;
        }
    }

    return response;
}

}  // namespace

int main() {
    log_init(MODULE_NAME);
    LOG_I("Version: %lu.%lu.%lu", EMBER_VERSION, EMBER_SUBVERSION, EMBER_REVISION);

    try {
        openember::framework::InitSystemClient();
        auto node = openember::CreateNode(MODULE_NAME);

        auto node_info_pub =
            node->Advertise<openember::msgs::node::v1::NodeInfo>(kNodeInfoTopic);
        auto heartbeat_pub =
            node->Advertise<openember::msgs::node::v1::NodeHeartbeat>(kHeartbeatTopic);
        auto device_info_pub =
            node->Advertise<openember::msgs::device::v1::DeviceInfo>(kDeviceInfoTopic);
        auto device_state_pub =
            node->Advertise<openember::msgs::device::v1::DeviceState>(kDeviceStateTopic);

        auto devices = BuildInitialDevices();
        std::uint64_t query_sequence = 0;
        auto query_service = node->CreateService<
            openember::msgs::device::v1::DeviceQuery,
            openember::msgs::device::v1::DeviceQueryResponse>(
            kDeviceQueryService,
            [&](const openember::msgs::device::v1::DeviceQuery& request) {
                return BuildDeviceQueryResponse(request, devices, query_sequence++);
            });

        const auto start = std::chrono::steady_clock::now();
        const auto start_time_unix_ns = UnixTimeNs();
        std::uint64_t sequence = 0;
        std::uint64_t info_sequence = 0;

        while (openember::Ok()) {
            if (sequence % kNodeInfoPublishInterval == 0) {
                (void)node_info_pub.Publish(BuildNodeInfo(info_sequence, start_time_unix_ns));
                for (auto& device : devices) {
                    FillHeader(device.info.mutable_header(), sequence);
                    (void)device_info_pub.Publish(device.info);
                }
                ++info_sequence;
            }

            const auto uptime = std::chrono::steady_clock::now() - start;
            openember::msgs::node::v1::NodeHeartbeat heartbeat;
            FillHeader(heartbeat.mutable_header(), sequence);
            heartbeat.set_node_name(MODULE_NAME);
            heartbeat.set_instance_id(kInstanceId);
            heartbeat.set_lifecycle_state(
                openember::msgs::lifecycle::v1::LIFECYCLE_STATE_ACTIVE);
            heartbeat.set_health(openember::msgs::common::v1::HEALTH_STATE_OK);
            heartbeat.set_uptime_ms(static_cast<std::uint64_t>(
                std::chrono::duration_cast<std::chrono::milliseconds>(uptime).count()));
            heartbeat.set_status_message("managing devices");

            auto* devices_metric = heartbeat.add_metrics();
            devices_metric->set_name("device_manager.devices");
            devices_metric->set_value(static_cast<double>(devices.size()));
            devices_metric->set_unit("count");

            (void)heartbeat_pub.Publish(heartbeat);

            for (auto& device : devices) {
                FillHeader(device.state.mutable_header(), sequence);
                (void)device_state_pub.Publish(device.state);
            }

            ++sequence;
            std::this_thread::sleep_for(kHeartbeatPeriod);
        }

        openember::Shutdown();
        log_deinit();
        return 0;
    } catch (const std::exception& e) {
        LOG_E("device_manager failed: %s", e.what());
        openember::Shutdown();
        log_deinit();
        return 1;
    }
}
