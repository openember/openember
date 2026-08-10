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

#define MODULE_NAME "health_monitor"
#define LOG_TAG MODULE_NAME
#include "openember.h"

#include "openember/framework/system_bus.hpp"
#include "openember/init.hpp"
#include "openember/msgs/common/v1/common.pb.h"
#include "openember/msgs/diagnostics/v1/diagnostics.pb.h"
#include "openember/msgs/lifecycle/v1/lifecycle.pb.h"
#include "openember/msgs/node/v1/node.pb.h"
#include "openember/node.hpp"

namespace {

constexpr const char* kRobotId = "openember";
constexpr const char* kInstanceId = MODULE_NAME;
constexpr const char* kNodeInfoTopic = "/nodes/health_monitor/info";
constexpr const char* kHeartbeatTopic = "/nodes/health_monitor/heartbeat";
constexpr const char* kAllHeartbeatsTopic = "/nodes/*/heartbeat";
constexpr const char* kDiagnosticsTopic = "/diagnostics/health_monitor";
constexpr auto kHeartbeatPeriod = std::chrono::seconds(1);
constexpr auto kNodeStaleTimeout = std::chrono::seconds(5);
constexpr std::uint64_t kNodeInfoPublishInterval = 10;

std::uint64_t UnixTimeNs() {
    const auto now = std::chrono::system_clock::now().time_since_epoch();
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(now).count());
}

void FillHeader(openember::msgs::common::v1::Header* header,
                std::uint64_t sequence) {
    header->set_source_node(MODULE_NAME);
    header->set_source_instance(kInstanceId);
    header->set_robot_id(kRobotId);
    header->set_sequence(sequence);
    header->set_timestamp_unix_ns(UnixTimeNs());
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
             kAllHeartbeatsTopic,
             "openember.msgs.node.v1.NodeHeartbeat",
             openember::msgs::node::v1::ENDPOINT_DIRECTION_SUBSCRIBER);
    AddTopic(&info,
             kDiagnosticsTopic,
             "openember.msgs.diagnostics.v1.DiagnosticArray",
             openember::msgs::node::v1::ENDPOINT_DIRECTION_PUBLISHER);

    AddLabel(&info, "role", "system");
    info.add_capabilities("node_heartbeat_aggregation");
    info.add_capabilities("diagnostics_publisher");
    return info;
}

class HealthRegistry {
public:
    void Update(const openember::msgs::node::v1::NodeHeartbeat& heartbeat) {
        if (heartbeat.node_name().empty()) {
            return;
        }
        std::lock_guard<std::mutex> lock(mutex_);
        auto& node = nodes_[heartbeat.node_name()];
        node.node_name = heartbeat.node_name();
        node.instance_id = heartbeat.instance_id();
        node.health = heartbeat.health();
        node.lifecycle_state = heartbeat.lifecycle_state();
        node.status_message = heartbeat.status_message();
        node.last_seen = std::chrono::steady_clock::now();
    }

    std::vector<openember::msgs::diagnostics::v1::DiagnosticStatus> Snapshot() const {
        const auto now = std::chrono::steady_clock::now();
        std::vector<openember::msgs::diagnostics::v1::DiagnosticStatus> result;

        std::lock_guard<std::mutex> lock(mutex_);
        for (const auto& [name, node] : nodes_) {
            const bool stale = now - node.last_seen > kNodeStaleTimeout;

            openember::msgs::diagnostics::v1::DiagnosticStatus status;
            status.set_name("node." + name);
            status.set_node_name(name);
            status.set_hardware_id("runtime");
            status.set_level(stale
                ? openember::msgs::diagnostics::v1::DIAGNOSTIC_LEVEL_STALE
                : ToDiagnosticLevel(node.health));
            status.set_message(stale ? "node heartbeat stale" : node.status_message);

            auto* instance = status.add_values();
            instance->set_key("instance_id");
            instance->set_value(node.instance_id);

            auto* lifecycle = status.add_values();
            lifecycle->set_key("lifecycle_state");
            lifecycle->set_value(std::to_string(static_cast<int>(node.lifecycle_state)));

            auto* last_seen = status.add_metrics();
            last_seen->set_name("node.heartbeat.age");
            last_seen->set_value(static_cast<double>(
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    now - node.last_seen).count()));
            last_seen->set_unit("ms");

            result.push_back(std::move(status));
        }

        return result;
    }

private:
    static openember::msgs::diagnostics::v1::DiagnosticLevel ToDiagnosticLevel(
        openember::msgs::common::v1::HealthState health) {
        using HealthState = openember::msgs::common::v1::HealthState;
        using DiagnosticLevel = openember::msgs::diagnostics::v1::DiagnosticLevel;

        switch (health) {
            case HealthState::HEALTH_STATE_OK:
                return DiagnosticLevel::DIAGNOSTIC_LEVEL_OK;
            case HealthState::HEALTH_STATE_WARNING:
                return DiagnosticLevel::DIAGNOSTIC_LEVEL_WARNING;
            case HealthState::HEALTH_STATE_ERROR:
                return DiagnosticLevel::DIAGNOSTIC_LEVEL_ERROR;
            case HealthState::HEALTH_STATE_STALE:
                return DiagnosticLevel::DIAGNOSTIC_LEVEL_STALE;
            case HealthState::HEALTH_STATE_UNSPECIFIED:
                break;
        }
        return DiagnosticLevel::DIAGNOSTIC_LEVEL_UNSPECIFIED;
    }

    struct NodeHealth {
        std::string node_name;
        std::string instance_id;
        openember::msgs::common::v1::HealthState health =
            openember::msgs::common::v1::HEALTH_STATE_UNSPECIFIED;
        openember::msgs::lifecycle::v1::LifecycleState lifecycle_state =
            openember::msgs::lifecycle::v1::LIFECYCLE_STATE_UNSPECIFIED;
        std::string status_message;
        std::chrono::steady_clock::time_point last_seen;
    };

    mutable std::mutex mutex_;
    std::map<std::string, NodeHealth> nodes_;
};

}  // namespace

int main() {
    log_init(MODULE_NAME);
    LOG_I("Version: %lu.%lu.%lu", EMBER_VERSION, EMBER_SUBVERSION, EMBER_REVISION);

    try {
        openember::framework::InitSystemClient();
        auto node = openember::CreateNode(MODULE_NAME);
        auto info_pub =
            node->Advertise<openember::msgs::node::v1::NodeInfo>(kNodeInfoTopic);
        auto heartbeat_pub =
            node->Advertise<openember::msgs::node::v1::NodeHeartbeat>(kHeartbeatTopic);
        auto diagnostics_pub =
            node->Advertise<openember::msgs::diagnostics::v1::DiagnosticArray>(
                kDiagnosticsTopic);

        HealthRegistry registry;
        auto heartbeat_sub =
            node->Subscribe<openember::msgs::node::v1::NodeHeartbeat>(
                kAllHeartbeatsTopic,
                [&](const openember::msgs::node::v1::NodeHeartbeat& heartbeat) {
                    registry.Update(heartbeat);
                });

        const auto start = std::chrono::steady_clock::now();
        const auto start_time_unix_ns = UnixTimeNs();
        std::uint64_t sequence = 0;
        std::uint64_t info_sequence = 0;

        while (openember::Ok()) {
            if (sequence % kNodeInfoPublishInterval == 0) {
                (void)info_pub.Publish(BuildNodeInfo(info_sequence, start_time_unix_ns));
                ++info_sequence;
            }

            const auto uptime = std::chrono::steady_clock::now() - start;
            const auto diagnostics = registry.Snapshot();

            openember::msgs::node::v1::NodeHeartbeat heartbeat;
            FillHeader(heartbeat.mutable_header(), sequence);
            heartbeat.set_node_name(MODULE_NAME);
            heartbeat.set_instance_id(kInstanceId);
            heartbeat.set_lifecycle_state(
                openember::msgs::lifecycle::v1::LIFECYCLE_STATE_ACTIVE);
            heartbeat.set_health(openember::msgs::common::v1::HEALTH_STATE_OK);
            heartbeat.set_uptime_ms(static_cast<std::uint64_t>(
                std::chrono::duration_cast<std::chrono::milliseconds>(uptime).count()));
            heartbeat.set_status_message("monitoring node heartbeats");

            auto* observed_metric = heartbeat.add_metrics();
            observed_metric->set_name("health_monitor.observed_nodes");
            observed_metric->set_value(static_cast<double>(diagnostics.size()));
            observed_metric->set_unit("count");

            (void)heartbeat_pub.Publish(heartbeat);

            openember::msgs::diagnostics::v1::DiagnosticArray diagnostic_array;
            FillHeader(diagnostic_array.mutable_header(), sequence);
            for (const auto& status : diagnostics) {
                *diagnostic_array.add_status() = status;
            }
            (void)diagnostics_pub.Publish(diagnostic_array);

            ++sequence;
            std::this_thread::sleep_for(kHeartbeatPeriod);
        }

        openember::Shutdown();
        log_deinit();
        return 0;
    } catch (const std::exception& e) {
        LOG_E("health_monitor failed: %s", e.what());
        openember::Shutdown();
        log_deinit();
        return 1;
    }
}
