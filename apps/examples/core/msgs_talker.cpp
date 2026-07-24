#include <chrono>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>

#include "openember/init.hpp"
#include "openember/node.hpp"
#include "openember/transport/buffer.hpp"
#include "openember/transport/options.hpp"
#include "openember/msgs/common/v1/common.pb.h"
#include "openember/msgs/lifecycle/v1/lifecycle.pb.h"
#include "openember/msgs/node/v1/node.pb.h"

namespace {

std::uint64_t UnixTimeNs() {
    const auto now = std::chrono::system_clock::now().time_since_epoch();
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(now).count());
}

openember::transport::ByteBuffer SerializeHeartbeat(
    const openember::msgs::node::v1::NodeHeartbeat& heartbeat) {
    std::string bytes;
    if (!heartbeat.SerializeToString(&bytes)) {
        throw std::runtime_error("failed to serialize NodeHeartbeat");
    }

    return openember::transport::ByteBuffer(bytes.begin(), bytes.end());
}

}  // namespace

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    openember::ContextOptions options;
    options.device_id = "demo";
    options.zenoh_connect = openember::transport::kDefaultZenohListenEndpoint;

    openember::Init(options);

    // Start openember_msgs_listener first.
    std::this_thread::sleep_for(std::chrono::seconds(1));

    auto node = openember::CreateNode("msgs_talker");
    auto pub = node->CreatePublisher<openember::transport::ByteBuffer>(
        "/msgs/heartbeat");

    std::uint64_t sequence = 0;
    const auto start = std::chrono::steady_clock::now();

    while (openember::Ok()) {
        openember::msgs::node::v1::NodeHeartbeat heartbeat;

        auto* header = heartbeat.mutable_header();
        header->set_source_node("msgs_talker");
        header->set_source_instance("demo-instance");
        header->set_robot_id("demo");
        header->set_namespace_name("");
        header->set_sequence(sequence);
        header->set_timestamp_unix_ns(UnixTimeNs());

        const auto uptime = std::chrono::steady_clock::now() - start;
        heartbeat.set_node_name("msgs_talker");
        heartbeat.set_instance_id("demo-instance");
        heartbeat.set_lifecycle_state(
            openember::msgs::lifecycle::v1::LIFECYCLE_STATE_ACTIVE);
        heartbeat.set_health(
            openember::msgs::common::v1::HEALTH_STATE_OK);
        heartbeat.set_uptime_ms(static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(uptime).count()));
        heartbeat.set_error_count(0);
        heartbeat.set_status_message("openember-msgs protobuf heartbeat");

        auto* metric = heartbeat.add_metrics();
        metric->set_name("loop.sequence");
        metric->set_value(static_cast<double>(sequence));
        metric->set_unit("count");

        const auto payload = SerializeHeartbeat(heartbeat);
        if (pub.Publish(payload)) {
            std::cout << "publish NodeHeartbeat sequence="
                      << sequence
                      << " bytes="
                      << payload.size()
                      << std::endl;
        } else {
            std::cerr << "publish failed" << std::endl;
        }

        ++sequence;
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    openember::Shutdown();
    return 0;
}
