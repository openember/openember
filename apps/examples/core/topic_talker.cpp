#include <chrono>
#include <iostream>
#include <string>
#include <thread>

#include "openember/init.hpp"
#include "openember/link/options.hpp"
#include "openember/node.hpp"

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    openember::RuntimeOptions options;
    options.robot_id = "demo";
    options.link = openember::link::LocalClientOptions();

    openember::Init(options);

    // 先启动 openember_topic_listener
    std::this_thread::sleep_for(std::chrono::seconds(1));

    auto node = openember::CreateNode("talker");
    auto pub = node->Advertise<std::string>("/chatter");

    int count = 0;

    while (openember::Ok()) {
        std::string msg = "hello openember " + std::to_string(count++);

        if (pub.Publish(msg)) {
            std::cout << "publish: " << msg << std::endl;
        } else {
            std::cerr << "publish failed" << std::endl;
        }

        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    openember::Shutdown();
    return 0;
}
