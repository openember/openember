/*
 * Copyright (c) 2022-2026, OpenEmber Team
 * SPDX-License-Identifier: Apache-2.0
 *
 * Optional runtime service: sample host CPU/memory/disk and publish
 * HostResourceSnapshot on /host/resources for web_console and peers.
 */

#include <unistd.h>

#include <chrono>
#include <cstdint>
#include <exception>
#include <iostream>
#include <string>
#include <thread>

#define MODULE_NAME "host_monitor"
#define LOG_TAG MODULE_NAME
#include "openember.h"

#include "host_sampler.hpp"

#include "openember/framework/system_bus.hpp"
#include "openember/init.hpp"
#include "openember/msgs/common/v1/common.pb.h"
#include "openember/msgs/host/v1/host.pb.h"
#include "openember/msgs/lifecycle/v1/lifecycle.pb.h"
#include "openember/msgs/node/v1/node.pb.h"
#include "openember/node.hpp"

namespace {

constexpr const char* kRobotId = "openember";
constexpr const char* kInstanceId = MODULE_NAME;
constexpr const char* kNodeInfoTopic = "/nodes/host_monitor/info";
constexpr const char* kHeartbeatTopic = "/nodes/host_monitor/heartbeat";
constexpr const char* kResourcesTopic = "/host/resources";
constexpr auto kPublishPeriod = std::chrono::seconds(1);
constexpr std::uint64_t kNodeInfoPublishInterval = 10;

std::uint64_t UnixTimeNs() {
    const auto now = std::chrono::system_clock::now().time_since_epoch();
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(now).count());
}

void FillHeader(openember::msgs::common::v1::Header* header, std::uint64_t sequence) {
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

openember::msgs::node::v1::NodeInfo BuildNodeInfo(std::uint64_t sequence,
                                                  std::uint64_t start_time_unix_ns) {
    openember::msgs::node::v1::NodeInfo info;
    FillHeader(info.mutable_header(), sequence);
    info.set_node_name(MODULE_NAME);
    info.set_instance_id(kInstanceId);
    info.set_process_name(MODULE_NAME);
    info.set_process_id(static_cast<std::uint32_t>(getpid()));
    info.set_kind(openember::msgs::node::v1::NODE_KIND_SERVICE);
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
             kResourcesTopic,
             "openember.msgs.host.v1.HostResourceSnapshot",
             openember::msgs::node::v1::ENDPOINT_DIRECTION_PUBLISHER);

    auto* label = info.add_labels();
    label->set_key("role");
    label->set_value("host_monitor");
    info.add_capabilities("host_resources");
    return info;
}

void AddMetric(openember::msgs::host::v1::HostResourceSnapshot* snap,
               const char* name,
               double value,
               const char* unit) {
    auto* m = snap->add_metrics();
    m->set_name(name);
    m->set_value(value);
    m->set_unit(unit);
}

openember::msgs::host::v1::HostResourceSnapshot ToProto(
    const openember::host_monitor::HostSample& sample,
    std::uint64_t sequence) {
    openember::msgs::host::v1::HostResourceSnapshot msg;
    FillHeader(msg.mutable_header(), sequence);
    msg.set_uptime_sec(sample.uptime_sec);
    msg.set_load1(sample.load1);
    msg.set_load5(sample.load5);
    msg.set_load15(sample.load15);

    auto* cpu = msg.mutable_cpu();
    cpu->set_usage_ratio(sample.cpu.usage_ratio);
    cpu->set_online_cpus(sample.cpu.online_cpus);
    for (double core : sample.cpu.core_usage_ratio) {
        cpu->add_core_usage_ratio(core);
    }

    auto* memory = msg.mutable_memory();
    memory->set_total_bytes(sample.memory.total_bytes);
    memory->set_available_bytes(sample.memory.available_bytes);
    memory->set_used_bytes(sample.memory.used_bytes);
    memory->set_swap_total_bytes(sample.memory.swap_total_bytes);
    memory->set_swap_used_bytes(sample.memory.swap_used_bytes);

    for (const auto& disk : sample.disks) {
        auto* d = msg.add_disks();
        d->set_mount_point(disk.mount_point);
        d->set_filesystem(disk.filesystem);
        d->set_total_bytes(disk.total_bytes);
        d->set_used_bytes(disk.used_bytes);
        d->set_available_bytes(disk.available_bytes);
    }

    AddMetric(&msg, "cpu.usage_percent", sample.cpu.usage_ratio * 100.0, "percent");
    if (sample.memory.total_bytes > 0) {
        const double mem_pct =
            100.0 * static_cast<double>(sample.memory.used_bytes) /
            static_cast<double>(sample.memory.total_bytes);
        AddMetric(&msg, "memory.usage_percent", mem_pct, "percent");
    }
    AddMetric(&msg, "load.1", sample.load1, "load");
    return msg;
}

}  // namespace

int main() {
    log_init(MODULE_NAME);

    try {
        openember::framework::InitSystemClient();
        auto node = openember::CreateNode(MODULE_NAME);

        auto info_pub =
            node->Advertise<openember::msgs::node::v1::NodeInfo>(kNodeInfoTopic);
        auto heartbeat_pub =
            node->Advertise<openember::msgs::node::v1::NodeHeartbeat>(kHeartbeatTopic);
        auto resources_pub =
            node->Advertise<openember::msgs::host::v1::HostResourceSnapshot>(
                kResourcesTopic);

        openember::host_monitor::HostSampler sampler;
        const auto start = std::chrono::steady_clock::now();
        const auto start_unix_ns = UnixTimeNs();
        std::uint64_t sequence = 0;
        std::uint64_t info_sequence = 0;

        LOG_I("publishing HostResourceSnapshot on %s every %lld s",
              kResourcesTopic,
              static_cast<long long>(kPublishPeriod.count()));

        // Prime CPU counters so the first published sample can include usage.
        (void)sampler.Sample();
        std::this_thread::sleep_for(std::chrono::milliseconds(200));

        while (openember::Ok()) {
            const auto sample = sampler.Sample();
            auto snapshot = ToProto(sample, sequence);
            if (!resources_pub.Publish(snapshot)) {
                LOG_W("publish %s failed", kResourcesTopic);
            }

            openember::msgs::node::v1::NodeHeartbeat heartbeat;
            FillHeader(heartbeat.mutable_header(), sequence);
            heartbeat.set_node_name(MODULE_NAME);
            heartbeat.set_instance_id(kInstanceId);
            heartbeat.set_lifecycle_state(
                openember::msgs::lifecycle::v1::LIFECYCLE_STATE_ACTIVE);
            heartbeat.set_health(openember::msgs::common::v1::HEALTH_STATE_OK);
            const auto uptime = std::chrono::steady_clock::now() - start;
            heartbeat.set_uptime_ms(static_cast<std::uint64_t>(
                std::chrono::duration_cast<std::chrono::milliseconds>(uptime).count()));
            heartbeat.set_status_message("host resources ok");
            auto* metric = heartbeat.add_metrics();
            metric->set_name("cpu.usage_ratio");
            metric->set_value(sample.cpu.usage_ratio);
            metric->set_unit("ratio");
            (void)heartbeat_pub.Publish(heartbeat);

            if (sequence % kNodeInfoPublishInterval == 0) {
                auto info = BuildNodeInfo(info_sequence++, start_unix_ns);
                (void)info_pub.Publish(info);
            }

            if (sequence % 5 == 0) {
                LOG_I("cpu=%.1f%% mem_used=%llu/%llu load1=%.2f disks=%zu",
                      sample.cpu.usage_ratio * 100.0,
                      static_cast<unsigned long long>(sample.memory.used_bytes),
                      static_cast<unsigned long long>(sample.memory.total_bytes),
                      sample.load1,
                      sample.disks.size());
            }

            ++sequence;
            std::this_thread::sleep_for(kPublishPeriod);
        }

        openember::Shutdown();
        log_deinit();
        return 0;
    } catch (const std::exception& e) {
        LOG_E("host_monitor failed: %s", e.what());
        log_deinit();
        return 1;
    }
}
