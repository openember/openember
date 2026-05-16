#include <chrono>
#include <iostream>
#include <string>
#include <thread>

#include "openember/init.hpp"
#include "openember/node.hpp"
#include "openember/transport/options.hpp"

#include <thread>

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    openember::ContextOptions options;
    options.device_id = "demo";
    options.zenoh_connect = openember::transport::kDefaultZenohListenEndpoint;

    openember::Init(options);

    // 先启动 openember_topic_listener
    std::this_thread::sleep_for(std::chrono::seconds(1));

    auto node = openember::CreateNode("talker");
    auto pub = node->CreatePublisher<std::string>("/chatter");

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