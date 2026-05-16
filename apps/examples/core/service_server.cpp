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

    auto node = openember::CreateNode("echo_server");

    auto srv = node->CreateService<std::string, std::string>(
        "/echo",
        [](const std::string& request) {
            std::cout << "request: " << request << std::endl;
            return "echo: " + request;
        });

    std::cout << "echo service ready. press Enter to exit" << std::endl;
    std::cin.get();

    openember::Shutdown();
    return 0;
}