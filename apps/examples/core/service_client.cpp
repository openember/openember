#include <chrono>
#include <iostream>
#include <string>

#include "openember/init.hpp"
#include "openember/link/options.hpp"
#include "openember/node.hpp"

#include <thread>

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    openember::RuntimeOptions options;
    options.robot_id = "demo";
    options.link = openember::link::LocalClientOptions();

    openember::Init(options);

    std::this_thread::sleep_for(std::chrono::seconds(1));

    auto node = openember::CreateNode("echo_client");
    auto client =
        node->CreateClient<std::string, std::string>("/echo");

    auto response = client.Call(
        "hello service",
        std::chrono::milliseconds(1000));

    if (!response.has_value()) {
        std::cerr << "service call failed or timeout" << std::endl;
        openember::Shutdown();
        return 1;
    }

    std::cout << "response: " << response.value() << std::endl;

    openember::Shutdown();
    return 0;
}
