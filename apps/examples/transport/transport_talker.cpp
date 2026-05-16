#include <chrono>
#include <iostream>
#include <string>
#include <thread>

#include "openember/transport/options.hpp"
#include "openember/transport/session.hpp"

int main() {
    using namespace openember::transport;

    SessionOptions options;
    options.robot_id = "demo";
    // 连接到 listener 已监听的端口（请先启动 transport_listener）。
    options.connect = kDefaultZenohListenEndpoint;

    Session session(options);

    auto open_result = session.Open();
    if (!open_result.Ok()) {
        std::cerr << open_result.Err().message << std::endl;
        return 1;
    }

    // 等待 listener 完成 listen 并建立 TCP（先启动 transport_listener）。
    std::this_thread::sleep_for(std::chrono::seconds(1));

    const std::string topic_key = session.Keys().TopicKey("/chatter");
    std::cout << "publishing to: " << topic_key << std::endl;

    auto pub_result = session.CreatePublisher({.name = "/chatter"});
    if (!pub_result.Ok()) {
        std::cerr << pub_result.Err().message << std::endl;
        return 1;
    }

    auto pub = std::move(pub_result.Value());

    int count = 0;
    while (true) {
        std::string text = "hello openember " + std::to_string(count++);
        ByteBuffer buffer(text.begin(), text.end());

        auto result = pub.Publish(buffer);
        if (!result.Ok()) {
            std::cerr << result.Err().message << std::endl;
        } else {
            std::cout << "publish: " << text << std::endl;
        }

        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    return 0;
}