#include <iostream>

#include "openember/transport/options.hpp"
#include "openember/transport/session.hpp"

int main() {
    using namespace openember::transport;

    SessionOptions options;
    options.robot_id = "demo";
    // zenoh-c：listen 端须为 router，connect 端为 peer（见 zenoh-cpp connectivity 测试）。
    options.mode = ZenohMode::kRouter;
    options.listen = kDefaultZenohListenEndpoint;

    Session session(options);

    auto open_result = session.Open();
    if (!open_result.Ok()) {
        std::cerr << open_result.Err().message << std::endl;
        return 1;
    }

    const std::string topic_key = session.Keys().TopicKey("/chatter");
    std::cout << "subscribing to: " << topic_key << std::endl;

    auto sub_result = session.CreateSubscriber(
        {.name = "/chatter"},
        [](const Message& msg) {
            std::string text(msg.payload.begin(), msg.payload.end());
            std::cout << "recv [" << msg.key << "]: " << text << std::endl;
        }
    );

    if (!sub_result.Ok()) {
        std::cerr << sub_result.Err().message << std::endl;
        return 1;
    }

    auto sub = std::move(sub_result.Value());

    std::cout << "listening on " << kDefaultZenohListenEndpoint
              << " ... press Enter to exit" << std::endl;
    std::cin.get();

    return 0;
}