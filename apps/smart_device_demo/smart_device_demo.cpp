#include <chrono>
#include <cstdint>
#include <exception>
#include <iostream>
#include <string>
#include <thread>

#include "openember/init.hpp"
#include "openember/link/options.hpp"
#include "openember/msgs/common/v1/common.pb.h"
#include "openember/msgs/lifecycle/v1/lifecycle.pb.h"
#include "openember/msgs/node/v1/node.pb.h"
#include "openember/node.hpp"

namespace {

constexpr const char* kRobotId = "smart-device-demo";
constexpr const char* kInstanceId = "demo-instance";
constexpr const char* kNodeName = "smart_device_app";
constexpr const char* kHeartbeatTopic = "/product/smart_device/heartbeat";

enum class LinkMode {
    kAuto,
    kRouter,
    kClient,
};

std::uint64_t UnixTimeNs() {
    const auto now = std::chrono::system_clock::now().time_since_epoch();
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(now).count());
}

LinkMode ParseLinkMode(int argc, char** argv) {
    for (int i = 1; i < argc; ++i) {
        const std::string arg(argv[i]);
        if (arg == "--router") {
            return LinkMode::kRouter;
        }
        if (arg == "--client") {
            return LinkMode::kClient;
        }
    }
    return LinkMode::kAuto;
}

void InitRouter() {
    openember::RuntimeOptions options;
    options.robot_id = kRobotId;
    options.link = openember::link::LocalRouterOptions();
    openember::Init(options);
}

void InitClient() {
    openember::RuntimeOptions options;
    options.robot_id = kRobotId;
    options.link = openember::link::LocalClientOptions();
    openember::Init(options);
}

std::string InitLink(LinkMode mode) {
    if (mode == LinkMode::kRouter) {
        InitRouter();
        return "router";
    }

    if (mode == LinkMode::kClient) {
        InitClient();
        return "client";
    }

    try {
        InitRouter();
        return "router";
    } catch (const std::exception&) {
        openember::Shutdown();
        InitClient();
        return "client";
    }
}

}  // namespace

int main(int argc, char** argv) {
    try {
        const auto link_mode = InitLink(ParseLinkMode(argc, argv));

        auto node = openember::CreateNode(kNodeName);
        auto heartbeat_pub =
            node->Advertise<openember::msgs::node::v1::NodeHeartbeat>(
                kHeartbeatTopic);

        const auto start = std::chrono::steady_clock::now();
        std::uint64_t sequence = 0;

        std::cout << node->Name()
                  << " started as Link "
                  << link_mode
                  << ", publishing NodeHeartbeat on "
                  << kHeartbeatTopic
                  << std::endl;

        while (openember::Ok()) {
            openember::msgs::node::v1::NodeHeartbeat heartbeat;

            auto* header = heartbeat.mutable_header();
            header->set_source_node(node->Name());
            header->set_source_instance(kInstanceId);
            header->set_robot_id(kRobotId);
            header->set_sequence(sequence);
            header->set_timestamp_unix_ns(UnixTimeNs());

            const auto uptime = std::chrono::steady_clock::now() - start;
            heartbeat.set_node_name(node->Name());
            heartbeat.set_instance_id(kInstanceId);
            heartbeat.set_lifecycle_state(
                openember::msgs::lifecycle::v1::LIFECYCLE_STATE_ACTIVE);
            heartbeat.set_health(openember::msgs::common::v1::HEALTH_STATE_OK);
            heartbeat.set_uptime_ms(static_cast<std::uint64_t>(
                std::chrono::duration_cast<std::chrono::milliseconds>(uptime).count()));
            heartbeat.set_error_count(0);
            heartbeat.set_status_message("smart device demo running");

            auto* uptime_metric = heartbeat.add_metrics();
            uptime_metric->set_name("app.uptime");
            uptime_metric->set_value(static_cast<double>(heartbeat.uptime_ms()));
            uptime_metric->set_unit("ms");

            auto* sequence_metric = heartbeat.add_metrics();
            sequence_metric->set_name("app.sequence");
            sequence_metric->set_value(static_cast<double>(sequence));
            sequence_metric->set_unit("count");

            if (heartbeat_pub.Publish(heartbeat)) {
                std::cout << node->Name()
                          << " publish NodeHeartbeat sequence="
                          << sequence
                          << std::endl;
            } else {
                std::cerr << node->Name()
                          << " publish NodeHeartbeat failed"
                          << std::endl;
            }

            ++sequence;
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }

        openember::Shutdown();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "smart_device_demo failed: " << e.what() << std::endl;
        openember::Shutdown();
        return 1;
    }
}
