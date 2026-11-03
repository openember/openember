// Host-only regression for empty Zenoh payloads and service timeout lifetime.
#include "openember/transport/session.hpp"
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <atomic>
#include <chrono>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <thread>

namespace {
using namespace std::chrono_literals;
namespace transport = openember::transport;
void Check(bool condition, const char* reason) {
    if (!condition) throw std::runtime_error(reason);
}
std::string Endpoint() {
    const int fd = socket(AF_INET, SOCK_STREAM, 0);
    Check(fd >= 0, "socket failed");
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    socklen_t size = sizeof(address);
    const bool ok = bind(fd, reinterpret_cast<sockaddr*>(&address), size) == 0 &&
        getsockname(fd, reinterpret_cast<sockaddr*>(&address), &size) == 0;
    close(fd);
    Check(ok, "loopback port allocation failed");
    // A competing binder can win this small allocation race; Open() then fails visibly.
    return "tcp/127.0.0.1:" + std::to_string(ntohs(address.sin_port));
}
struct State {
    std::atomic<bool> empty_topic{false}, empty_request{false}, delayed_done{false};
};
}
int main() {
    try {
        transport::SessionOptions router_options;
        router_options.mode = transport::ZenohMode::kRouter;
        router_options.listen = Endpoint();
        router_options.robot_id = "transport-regression";
        transport::Session router(router_options);
        Check(router.Open().Ok(), "router open failed");
        auto state = std::make_shared<State>();
        auto service = router.CreateServiceServer({"/echo"}, [state](const transport::Message& request)
                -> transport::Result<transport::ByteBuffer> {
            if (request.payload.empty()) state->empty_request = true;
            if (request.payload == transport::ByteBuffer{1}) {
                std::this_thread::sleep_for(250ms); // Deliberately later than Call's deadline.
                state->delayed_done = true;
            }
            return request.payload;
        });
        Check(service.Ok(), "service creation failed");
        auto publisher = router.CreatePublisher({"/empty"});
        Check(publisher.Ok(), "publisher creation failed");
        transport::SessionOptions client_options;
        client_options.mode = transport::ZenohMode::kClient;
        client_options.connect = router_options.listen;
        client_options.robot_id = router_options.robot_id;
        transport::Session client_session(client_options);
        Check(client_session.Open().Ok(), "client session open failed");
        auto subscriber = client_session.CreateSubscriber({"/empty"}, [state](const transport::Message& message) {
            if (message.payload.empty()) state->empty_topic = true;
        });
        Check(subscriber.Ok(), "subscriber creation failed");
        auto client = client_session.CreateServiceClient({"/echo"});
        Check(client.Ok(), "client creation failed");
        const auto deadline = std::chrono::steady_clock::now() + 5s;
        bool empty_reply = false;
        while (std::chrono::steady_clock::now() < deadline && (!state->empty_topic || !empty_reply)) {
            Check(publisher.Value().Publish({}).Ok(), "empty publication failed");
            auto response = client.Value().Call({}, 300ms);
            empty_reply = response.Ok() && response.Value().empty();
            std::this_thread::sleep_for(20ms);
        }
        Check(state->empty_topic && state->empty_request && empty_reply, "empty topic/request/reply failed");
        auto timed_out = client.Value().Call({1}, 40ms);
        Check(!timed_out.Ok() && timed_out.Err().code == transport::ErrorCode::kTimeout,
              "delayed call must time out");
        const auto late_deadline = std::chrono::steady_clock::now() + 2s;
        while (!state->delayed_done && std::chrono::steady_clock::now() < late_deadline)
            std::this_thread::sleep_for(10ms);
        Check(state->delayed_done, "delayed handler never executed");
        // Give the late query/reply closure time to run after the caller returned.
        std::this_thread::sleep_for(100ms);
        const transport::ByteBuffer payload(65536, 42);
        auto response = client.Value().Call(payload, 1s);
        Check(response.Ok() && response.Value() == payload, "service failed after timeout");
        std::cout << "empty topic/request/reply and timeout recovery passed\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
