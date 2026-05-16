#include <iostream>
#include <string>

#include "openember/init.hpp"
#include "openember/node.hpp"
#include "openember/transport/options.hpp"

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    openember::ContextOptions options;
    options.device_id = "demo";
    options.zenoh_mode = openember::transport::ZenohMode::kRouter;
    options.zenoh_listen = openember::transport::kDefaultZenohListenEndpoint;

    openember::Init(options);

    auto node = openember::CreateNode("listener");

    auto sub = node->CreateSubscriber<std::string>(
        "/chatter",
        [](const std::string& msg) {
            std::cout << "recv: " << msg << std::endl;
        });

    std::cout << "listening... press Enter to exit" << std::endl;
    std::cin.get();

    openember::Shutdown();
    return 0;
}