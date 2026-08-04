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

constexpr const char* kRobotId = "openember";
constexpr const char* kInstanceId = "demo-instance";
constexpr const char* kNodeName = "smart_device_app";
constexpr const char* kHeartbeatTopic = "/nodes/smart_device_app/heartbeat";

enum class LinkMode {
    kAuto,
    kRouter,
    kClient,
};

struct LinkCliOptions {
    LinkMode mode = LinkMode::kAuto;
    std::string connect = "tcp/127.0.0.1:7447";
    std::string listen = "tcp/127.0.0.1:7447";
};

std::uint64_t UnixTimeNs() {
    const auto now = std::chrono::system_clock::now().time_since_epoch();
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(now).count());
}

LinkCliOptions ParseLinkOptions(int argc, char** argv) {
    LinkCliOptions options;
    for (int i = 1; i < argc; ++i) {
        const std::string arg(argv[i]);
        if (arg == "--router") {
            options.mode = LinkMode::kRouter;
        } else if (arg == "--client") {
            options.mode = LinkMode::kClient;
        } else if (arg == "--connect" && i + 1 < argc) {
            options.connect = argv[++i];
        } else if (arg == "--listen" && i + 1 < argc) {
            options.listen = argv[++i];
        }
    }
    return options;
}

void InitRouter(const LinkCliOptions& cli) {
    openember::RuntimeOptions options;
    options.robot_id = kRobotId;
    options.link.profile = openember::link::Profile::kRouter;
    options.link.listen = cli.listen;
    openember::Init(options);
}

void InitClient(const LinkCliOptions& cli) {
    openember::RuntimeOptions options;
    options.robot_id = kRobotId;
    options.link.profile = openember::link::Profile::kClient;
    options.link.connect = cli.connect;
    openember::Init(options);
}

std::string InitLink(const LinkCliOptions& cli) {
    if (cli.mode == LinkMode::kRouter) {
        InitRouter(cli);
        return "router";
    }

    if (cli.mode == LinkMode::kClient) {
        InitClient(cli);
        return "client";
    }

    try {
        InitRouter(cli);
        return "router";
    } catch (const std::exception&) {
        openember::Shutdown();
        InitClient(cli);
        return "client";
    }
}

}  // namespace

int main(int argc, char** argv) {
    try {
        const auto link_mode = InitLink(ParseLinkOptions(argc, argv));

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
