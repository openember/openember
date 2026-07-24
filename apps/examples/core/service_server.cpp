#include <iostream>
#include <string>

#include "openember/init.hpp"
#include "openember/link/options.hpp"
#include "openember/node.hpp"

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    openember::RuntimeOptions options;
    options.robot_id = "demo";
    options.link = openember::link::LocalRouterOptions();

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
