// Copyright (c) 2022-2026, OpenEmber Team
// SPDX-License-Identifier: Apache-2.0
#include "management.hpp"
#include "network_sampler.hpp"
#include "openember/init.hpp"
#include "openember/launch.hpp"
#include "openember/link/options.hpp"
#include "openember/msgs/node/v1/node.pb.h"
#include "openember/node.hpp"
#include <chrono>
#include <csignal>
#include <iostream>
#include <random>
#include <stdexcept>
#include <thread>
#include <unistd.h>

namespace {
volatile std::sig_atomic_t stopping = 0;
void Stop(int) { stopping = 1; }
std::uint64_t UnixNs() {
  return std::chrono::duration_cast<std::chrono::nanoseconds>(
             std::chrono::system_clock::now().time_since_epoch())
      .count();
}
} // namespace
int main(int argc, char **argv) {
  try {
    std::string robot = "openember", host, connect = "tcp/127.0.0.1:7447";
    unsigned period = 1000;
    bool manage = false;
    std::string token_file;
    for (int i = 1; i < argc; ++i) {
      const std::string key = argv[i];
      if (key == "--manage") {
        manage = true;
        continue;
      }
      if (key == "--help") {
        std::cout << "network_manager [--robot ID] [--connect ENDPOINT] "
                     "[--host-id ID] [--period-ms 100..10000]\n"
                     "Kernel observation; --manage enables NetworkManager "
                     "management; --token-file PATH overrides local private "
                     "credential.\n";
        return 0;
      }
      if (++i == argc)
        throw std::invalid_argument("missing argument");
      const std::string value = argv[i];
      if (key == "--token-file")
        token_file = value;
      else if (key == "--robot")
        robot = value;
      else if (key == "--host-id")
        host = value;
      else if (key == "--connect")
        connect = value;
      else if (key == "--period-ms") {
        if (value.empty() ||
            value.find_first_not_of("0123456789") != std::string::npos)
          throw std::invalid_argument("invalid period");
        const auto parsed = std::stoul(value);
        if (parsed < 100 || parsed > 10000)
          throw std::invalid_argument("period must be 100..10000 ms");
        period = parsed;
      } else
        throw std::invalid_argument("unknown option: " + key);
    }
    if (host.empty()) {
      char name[128]{};
      if (gethostname(name, sizeof(name) - 1) || !name[0])
        throw std::runtime_error("hostname unavailable");
      host = name;
    }
    if (host.size() > 127 || robot.empty() || robot.size() > 127 ||
        connect.empty())
      throw std::invalid_argument("invalid host, robot or endpoint");
    if (manage && connect.rfind("tcp/127.0.0.1:", 0) != 0)
      throw std::invalid_argument(
          "Network management requires a local Link endpoint");
    const auto start = std::chrono::steady_clock::now();
    const auto start_unix = UnixNs();
    const auto instance = "network-" + std::to_string(start_unix) + "-" +
                          std::to_string(getpid()) + "-" +
                          std::to_string(std::random_device{}());
    std::signal(SIGINT, Stop);
    std::signal(SIGTERM, Stop);
    openember::RuntimeOptions options;
    options.robot_id = robot;
    options.link = openember::link::LocalClientOptions();
    options.link.connect = connect;
    openember::Init(options);
    auto node = openember::CreateNode("network_manager");
    std::shared_ptr<openember::network_manager::Management> management;
    if (manage) {
      management = std::make_shared<openember::network_manager::Management>(
          instance, host, token_file);
      node->CreateService<openember::network_manager::Request,
                          openember::network_manager::Reply>(
          "/network/rpc", [owner=management](const auto &r) { return owner->Call(r); });
    }
    auto publisher =
        node->Advertise<openember::network_manager::Snapshot>("/network/state");
    namespace n = openember::msgs::node::v1;
    auto info_pub = node->Advertise<n::NodeInfo>("/nodes/network_manager/info");
    auto heartbeat_pub =
        node->Advertise<n::NodeHeartbeat>("/nodes/network_manager/heartbeat");
    std::uint64_t sequence = 0;
    auto header = [&](auto *h) {
      h->set_robot_id(robot);
      h->set_source_node("network_manager");
      h->set_source_instance(instance);
      h->set_sequence(sequence);
      h->set_timestamp_unix_ns(UnixNs());
    };
    auto prepare = [&] {
      auto sample = openember::network_manager::Sample();
      if (management)
        management->Merge(sample);
      ++sequence;
      header(sample.mutable_header());
      sample.set_host_id(host);
      sample.set_online(true);
      sample.set_stale_after_ms(period * 3 + 500);
      return sample;
    };
    auto sample = prepare();
    if (!publisher.Publish(sample) || !openember::launch::NotifyReady())
      throw std::runtime_error("initial publication or readiness failed");
    std::cout << "NETWORK_MANAGER_READY host=" << host
              << " backend=linux-rtnetlink management=" << manage << "\n"
              << std::flush;
    while (!stopping && openember::Ok()) {
      n::NodeHeartbeat heartbeat;
      header(heartbeat.mutable_header());
      heartbeat.set_node_name("network_manager");
      heartbeat.set_instance_id(instance);
      heartbeat.set_lifecycle_state(
          openember::msgs::lifecycle::v1::LIFECYCLE_STATE_ACTIVE);
      heartbeat.set_health(
          sample.interfaces_valid() && sample.addresses_valid()
              ? openember::msgs::common::v1::HEALTH_STATE_OK
              : openember::msgs::common::v1::HEALTH_STATE_WARNING);
      heartbeat.set_uptime_ms(
          std::chrono::duration_cast<std::chrono::milliseconds>(
              std::chrono::steady_clock::now() - start)
              .count());
      heartbeat.set_status_message(sample.diagnostic().empty()
                                       ? "network observed; read-only"
                                       : sample.diagnostic());
      (void)heartbeat_pub.Publish(heartbeat);
      if (sequence == 1 || sequence % 10 == 0) {
        n::NodeInfo info;
        header(info.mutable_header());
        info.set_node_name("network_manager");
        info.set_instance_id(instance);
        info.set_process_name("network_manager");
        info.set_process_id(getpid());
        info.set_kind(n::NODE_KIND_SERVICE);
        info.set_version("0.1.0");
        info.set_start_time_unix_ns(start_unix);
        info.add_capabilities("network_observation");
        for (const auto &topic :
             {std::pair<const char *, const char *>{
                  "/network/state",
                  "openember.msgs.network.v1.NetworkStateSnapshot"},
              {"/nodes/network_manager/info",
               "openember.msgs.node.v1.NodeInfo"},
              {"/nodes/network_manager/heartbeat",
               "openember.msgs.node.v1.NodeHeartbeat"}}) {
          auto *endpoint = info.add_topics();
          endpoint->set_name(topic.first);
          endpoint->set_type(topic.second);
          endpoint->set_direction(n::ENDPOINT_DIRECTION_PUBLISHER);
          endpoint->mutable_qos()->set_queue_size(8);
          endpoint->mutable_qos()->set_reliability(
              openember::msgs::common::v1::RELIABILITY_RELIABLE);
          endpoint->mutable_qos()->set_durability(
              openember::msgs::common::v1::DURABILITY_VOLATILE);
        }
        (void)info_pub.Publish(info);
      }
      const auto deadline =
          std::chrono::steady_clock::now() + std::chrono::milliseconds(period);
      while (!stopping && openember::Ok() &&
             std::chrono::steady_clock::now() < deadline)
        std::this_thread::sleep_for(std::chrono::milliseconds(25));
      if (stopping || !openember::Ok())
        break;
      sample = prepare();
      if (!publisher.Publish(sample))
        std::cerr << "network publication failed\n";
    }
    sample = prepare();
    sample.set_online(false);
    (void)publisher.Publish(sample);
    openember::Shutdown();
    management.reset();
    return 0;
  } catch (const std::exception &error) {
    std::cerr << "NETWORK_MANAGER_ERROR " << error.what() << '\n';
    openember::Shutdown();
    return 1;
  }
}
