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

    auto node = openember::CreateNode("listener");

    auto sub = node->Subscribe<std::string>(
        "/chatter",
        [](const std::string& msg) {
            std::cout << "recv: " << msg << std::endl;
        });

    std::cout << "listening... press Enter to exit" << std::endl;
    std::cin.get();

    openember::Shutdown();
    return 0;
}
