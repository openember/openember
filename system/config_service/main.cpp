/*
 * Copyright (c) 2022-2026, OpenEmber Team
 * SPDX-License-Identifier: Apache-2.0
 */

#include <unistd.h>

#include <chrono>
#include <cstdint>
#include <exception>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#define MODULE_NAME "config_service"
#define LOG_TAG MODULE_NAME
#include "openember.h"

#include "openember/framework/system_bus.hpp"
#include "openember/init.hpp"
#include "openember/msgs/common/v1/common.pb.h"
#include "openember/msgs/lifecycle/v1/lifecycle.pb.h"
#include "openember/msgs/node/v1/node.pb.h"
#include "openember/msgs/parameter/v1/parameter.pb.h"
#include "openember/node.hpp"

namespace {

constexpr const char* kRobotId = "openember";
constexpr const char* kInstanceId = MODULE_NAME;
constexpr const char* kNodeInfoTopic = "/nodes/config_service/info";
constexpr const char* kHeartbeatTopic = "/nodes/config_service/heartbeat";
constexpr const char* kParameterEventsTopic = "/parameters/events";
constexpr const char* kGetParameterService = "/parameters/get";
constexpr const char* kSetParameterService = "/parameters/set";
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

openember::msgs::parameter::v1::Parameter MakeStringParameter(
    const std::string& name,
    const std::string& description,
    const std::string& value,
    bool read_only = false) {
    openember::msgs::parameter::v1::Parameter parameter;
    auto* definition = parameter.mutable_definition();
    definition->set_name(name);
    definition->set_type(openember::msgs::parameter::v1::PARAMETER_TYPE_STRING);
    definition->set_description(description);
    definition->set_read_only(read_only);
    definition->mutable_default_value()->set_type(
        openember::msgs::parameter::v1::PARAMETER_TYPE_STRING);
    definition->mutable_default_value()->set_string_value(value);

    parameter.mutable_value()->set_type(
        openember::msgs::parameter::v1::PARAMETER_TYPE_STRING);
    parameter.mutable_value()->set_string_value(value);
    return parameter;
}

class ParameterStore {
public:
    ParameterStore() {
        Put(MakeStringParameter("product.mode", "Current product mode", "demo"));
        Put(MakeStringParameter("runtime.robot_id", "OpenEmber robot id", kRobotId, true));
    }

    std::vector<openember::msgs::parameter::v1::Parameter> Get(
        const std::vector<std::string>& names) const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<openember::msgs::parameter::v1::Parameter> result;

        if (names.empty()) {
            for (const auto& [name, parameter] : parameters_) {
                (void)name;
                result.push_back(parameter);
            }
            return result;
        }

        for (const auto& name : names) {
            const auto it = parameters_.find(name);
            if (it != parameters_.end()) {
                result.push_back(it->second);
            }
        }
        return result;
    }

    bool Set(const std::vector<openember::msgs::parameter::v1::Parameter>& parameters,
             std::vector<openember::msgs::parameter::v1::Parameter>* changed,
             std::string* message) {
        std::lock_guard<std::mutex> lock(mutex_);
        for (const auto& parameter : parameters) {
            const auto& name = parameter.definition().name();
            if (name.empty()) {
                *message = "parameter name is required";
                return false;
            }

            const auto existing = parameters_.find(name);
            if (existing != parameters_.end() &&
                existing->second.definition().read_only()) {
                *message = "parameter is read-only: " + name;
                return false;
            }
        }

        for (const auto& parameter : parameters) {
            Put(parameter);
            changed->push_back(parameters_.at(parameter.definition().name()));
        }

        *message = "ok";
        return true;
    }

    std::size_t Size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return parameters_.size();
    }

private:
    void Put(const openember::msgs::parameter::v1::Parameter& parameter) {
        parameters_[parameter.definition().name()] = parameter;
    }

    mutable std::mutex mutex_;
    std::map<std::string, openember::msgs::parameter::v1::Parameter> parameters_;
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
             kParameterEventsTopic,
             "openember.msgs.parameter.v1.ParameterEvent",
             openember::msgs::node::v1::ENDPOINT_DIRECTION_PUBLISHER);
    AddService(&info,
               kGetParameterService,
               "openember.msgs.parameter.v1.GetParameterRequest",
               "openember.msgs.parameter.v1.GetParameterResponse");
    AddService(&info,
               kSetParameterService,
               "openember.msgs.parameter.v1.SetParameterRequest",
               "openember.msgs.parameter.v1.SetParameterResponse");

    AddLabel(&info, "role", "system");
    info.add_capabilities("parameter_get");
    info.add_capabilities("parameter_set");
    info.add_capabilities("parameter_events");
    return info;
}

openember::msgs::parameter::v1::GetParameterResponse BuildGetResponse(
    const openember::msgs::parameter::v1::GetParameterRequest& request,
    const std::vector<openember::msgs::parameter::v1::Parameter>& parameters,
    std::uint64_t sequence) {
    openember::msgs::parameter::v1::GetParameterResponse response;
    FillHeader(response.mutable_header(), sequence, &request.header());
    FillStatus(response.mutable_status(), true, "ok");
    for (const auto& parameter : parameters) {
        *response.add_parameters() = parameter;
    }
    return response;
}

openember::msgs::parameter::v1::SetParameterResponse BuildSetResponse(
    const openember::msgs::parameter::v1::SetParameterRequest& request,
    bool ok,
    const std::string& message,
    const std::vector<openember::msgs::parameter::v1::Parameter>& parameters,
    std::uint64_t sequence) {
    openember::msgs::parameter::v1::SetParameterResponse response;
    FillHeader(response.mutable_header(), sequence, &request.header());
    FillStatus(response.mutable_status(), ok, message);
    for (const auto& parameter : parameters) {
        *response.add_parameters() = parameter;
    }
    return response;
}

openember::msgs::parameter::v1::ParameterEvent BuildParameterEvent(
    const std::vector<openember::msgs::parameter::v1::Parameter>& parameters,
    std::uint64_t sequence) {
    openember::msgs::parameter::v1::ParameterEvent event;
    FillHeader(event.mutable_header(), sequence);
    event.set_node_name(MODULE_NAME);
    event.set_event_type(
        openember::msgs::parameter::v1::ParameterEvent::EVENT_TYPE_CHANGED);
    for (const auto& parameter : parameters) {
        *event.add_parameters() = parameter;
    }
    return event;
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
        auto parameter_event_pub =
            node->Advertise<openember::msgs::parameter::v1::ParameterEvent>(
                kParameterEventsTopic);

        ParameterStore store;
        std::uint64_t service_sequence = 0;
        std::uint64_t event_sequence = 0;

        auto get_service = node->CreateService<
            openember::msgs::parameter::v1::GetParameterRequest,
            openember::msgs::parameter::v1::GetParameterResponse>(
            kGetParameterService,
            [&](const openember::msgs::parameter::v1::GetParameterRequest& request) {
                std::vector<std::string> names;
                names.assign(request.names().begin(), request.names().end());
                return BuildGetResponse(request, store.Get(names), service_sequence++);
            });

        auto set_service = node->CreateService<
            openember::msgs::parameter::v1::SetParameterRequest,
            openember::msgs::parameter::v1::SetParameterResponse>(
            kSetParameterService,
            [&](const openember::msgs::parameter::v1::SetParameterRequest& request) {
                std::vector<openember::msgs::parameter::v1::Parameter> parameters;
                parameters.assign(request.parameters().begin(), request.parameters().end());

                std::vector<openember::msgs::parameter::v1::Parameter> changed;
                std::string message;
                const bool ok = store.Set(parameters, &changed, &message);
                if (ok && !changed.empty()) {
                    (void)parameter_event_pub.Publish(
                        BuildParameterEvent(changed, event_sequence++));
                }
                return BuildSetResponse(request, ok, message, changed, service_sequence++);
            });

        const auto start = std::chrono::steady_clock::now();
        const auto start_time_unix_ns = UnixTimeNs();
        std::uint64_t sequence = 0;
        std::uint64_t info_sequence = 0;

        while (openember::Ok()) {
            if (sequence % kNodeInfoPublishInterval == 0) {
                (void)node_info_pub.Publish(BuildNodeInfo(info_sequence, start_time_unix_ns));
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
            heartbeat.set_status_message("serving parameters");

            auto* parameters_metric = heartbeat.add_metrics();
            parameters_metric->set_name("config_service.parameters");
            parameters_metric->set_value(static_cast<double>(store.Size()));
            parameters_metric->set_unit("count");

            (void)heartbeat_pub.Publish(heartbeat);

            ++sequence;
            std::this_thread::sleep_for(kHeartbeatPeriod);
        }

        openember::Shutdown();
        log_deinit();
        return 0;
    } catch (const std::exception& e) {
        LOG_E("config_service failed: %s", e.what());
        openember::Shutdown();
        log_deinit();
        return 1;
    }
}
