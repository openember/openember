#include <cstdint>
#include <iostream>
#include <limits>

#include "openember/init.hpp"
#include "openember/node.hpp"
#include "openember/transport/buffer.hpp"
#include "openember/transport/options.hpp"
#include "openember/msgs/node/v1/node.pb.h"

namespace {

bool ParseHeartbeat(const openember::transport::ByteBuffer& payload,
                    openember::msgs::node::v1::NodeHeartbeat* heartbeat) {
    if (payload.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        return false;
    }

    return heartbeat->ParseFromArray(
        payload.data(),
        static_cast<int>(payload.size()));
}

}  // namespace

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    openember::ContextOptions options;
    options.device_id = "demo";
    options.zenoh_mode = openember::transport::ZenohMode::kRouter;
    options.zenoh_listen = openember::transport::kDefaultZenohListenEndpoint;

    openember::Init(options);

    auto node = openember::CreateNode("msgs_listener");

    auto sub = node->CreateSubscriber<openember::transport::ByteBuffer>(
        "/msgs/heartbeat",
        [](const openember::transport::ByteBuffer& payload) {
            openember::msgs::node::v1::NodeHeartbeat heartbeat;
            if (!ParseHeartbeat(payload, &heartbeat)) {
                std::cerr << "failed to parse NodeHeartbeat, bytes="
                          << payload.size()
                          << std::endl;
                return;
            }

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
