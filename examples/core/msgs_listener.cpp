#include <iostream>

#include "openember/init.hpp"
#include "openember/link/options.hpp"
#include "openember/node.hpp"
#include "openember/msgs/node/v1/node.pb.h"

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    openember::RuntimeOptions options;
    options.robot_id = "demo";
    options.link = openember::link::LocalRouterOptions();

    openember::Init(options);

    auto node = openember::CreateNode("msgs_listener");

    auto sub = node->Subscribe<openember::msgs::node::v1::NodeHeartbeat>(
        "/msgs/heartbeat",
        [](const openember::msgs::node::v1::NodeHeartbeat& heartbeat) {
            std::cout << "recv NodeHeartbeat"
                      << " sequence=" << heartbeat.header().sequence()
                      << " node=" << heartbeat.node_name()
                      << " health=" << heartbeat.health()
                      << " uptime_ms=" << heartbeat.uptime_ms()
                      << " status=\"" << heartbeat.status_message() << "\""
                      << std::endl;
        });

    std::cout << "listening for protobuf heartbeats... press Enter to exit"
              << std::endl;
    std::cin.get();

    openember::Shutdown();
    return 0;
}
