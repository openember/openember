#include "openember/alerts/link_publisher.hpp"
#include "openember/msgs/alerts/v1/alerts.pb.h"
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <condition_variable>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <thread>
using namespace openember::alerts;
using namespace std::chrono_literals;
namespace wire = openember::msgs::alerts::v1;
void Check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
std::string Endpoint() {
    int fd = socket(AF_INET, SOCK_STREAM, 0); Check(fd >= 0, "socket");
    sockaddr_in addr{}; addr.sin_family = AF_INET; addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    socklen_t length = sizeof(addr);
    bool good = bind(fd, reinterpret_cast<sockaddr*>(&addr), length) == 0 &&
        getsockname(fd, reinterpret_cast<sockaddr*>(&addr), &length) == 0;
    close(fd); Check(good, "allocate port");
    return "tcp/127.0.0.1:" + std::to_string(ntohs(addr.sin_port));
}
Event Make(std::uint64_t seq = 1, Transition transition = Transition::Raised) {
    Event e; e.key = {"motion", "robot-test", "control-1", "tilt"};
    e.sequence = seq; e.time_ns = seq * 100; e.transition = transition;
    e.clock_domain = "simulation"; e.value = .8; e.threshold = .7; e.clear_threshold = .6;
    e.unit = "rad"; e.reference_frame = "mujoco_world"; e.context = "locomotion";
    if (transition == Transition::Unknown) e.valid = false;
    return e;
}
Options Config() {
    Options o; o.logging.name = "alerts-link-test";
    o.logging.enable_file = o.logging.enable_topic = false; return o;
}
struct Received {
    std::mutex mutex;
    std::vector<wire::AlertEvent> events;
    std::vector<wire::AlertState> states;
};
struct Network {
    std::shared_ptr<openember::Context> router, client;
    std::shared_ptr<openember::Node> publisher, subscriber;
    std::shared_ptr<Received> received = std::make_shared<Received>();
    Network() {
        openember::ContextOptions server; server.device_id = "robot-test";
        server.zenoh_mode = openember::transport::ZenohMode::kRouter; server.zenoh_listen = Endpoint();
        router = std::make_shared<openember::Context>(server); Check(router->Init(), "router init");
        openember::ContextOptions peer; peer.device_id = server.device_id;
        peer.zenoh_mode = openember::transport::ZenohMode::kClient; peer.zenoh_connect = server.zenoh_listen;
        client = std::make_shared<openember::Context>(peer); Check(client->Init(), "client init");
        publisher = std::make_shared<openember::Node>(client, "test_alert_source");
    }
    void Subscribe() {
        subscriber = std::make_shared<openember::Node>(router, "test_state_manager");
        subscriber->CreateSubscriber<wire::AlertEvent>(kEventsTopic, [capture=received](const auto& e) {
            std::lock_guard<std::mutex> lock(capture->mutex); capture->events.push_back(e);
        });
        subscriber->CreateSubscriber<wire::AlertState>(kStateTopic, [capture=received](const auto& s) {
            std::lock_guard<std::mutex> lock(capture->mutex); capture->states.push_back(s);
        });
    }
    template<class Predicate> bool Wait(Predicate test) {
        const auto deadline = std::chrono::steady_clock::now() + 4s;
        while (std::chrono::steady_clock::now() < deadline) {
            { std::lock_guard<std::mutex> lock(received->mutex); if (test(*received)) return true; }
            std::this_thread::sleep_for(10ms);
        }
        return false;
    }
};
LinkOptions LinkConfig(const std::string& instance) {
    LinkOptions o; o.robot_id = "robot-test"; o.publisher_instance = instance;
    o.poll_interval = 5ms; o.state_interval = 100ms; o.stale_after_ms = 300; return o;
}
int main(int argc, char** argv) {
    try {
        Check(argc == 2, "case required"); const std::string name = argv[1]; Network network;
        if (name == "independent") {
            std::mutex mutex; std::condition_variable cv; bool blocked = false, release = false;
            auto options = Config();
            options.observer = [&](const Event&) {
                std::unique_lock<std::mutex> lock(mutex); blocked = true; cv.notify_all();
                cv.wait(lock, [&] { return release; });
            };
            Manager manager(options);
            // On any assertion failure, release the log dispatcher before Manager joins.
            struct Guard { std::mutex& mutex; std::condition_variable& cv; bool& release;
                ~Guard() { std::lock_guard<std::mutex> lock(mutex); release = true; cv.notify_all(); }
            } guard{mutex, cv, release};
            network.Subscribe(); LinkPublisher link(manager, network.publisher, LinkConfig(name));
            Check(network.Wait([](const auto& r) { return !r.states.empty(); }), "initial state missing");
            Check(manager.Publish(Make()) == PublishResult::Accepted, "publish");
            { std::unique_lock<std::mutex> lock(mutex); Check(cv.wait_for(lock, 2s, [&] { return blocked; }), "observer not blocked"); }
            Check(network.Wait([](const auto& r) { return !r.events.empty() && !r.states.empty() &&
                r.states.back().observations_size() == 1; }), "Link waited for log dispatcher");
            { std::lock_guard<std::mutex> lock(mutex); release = true; cv.notify_all(); }
            link.Stop(); manager.Stop();
        } else if (name == "lifecycle") {
            Manager manager(Config()); network.Subscribe();
            LinkPublisher link(manager, network.publisher, LinkConfig(name));
            Check(network.Wait([](const auto& r) { return !r.states.empty(); }), "initial empty state");
            for (const auto t : {Transition::Raised, Transition::Reminder, Transition::Unknown, Transition::Cleared})
                Check(manager.Publish(Make(manager.Stats().accepted + 1, t)) == PublishResult::Accepted, "publish");
            Check(network.Wait([](const auto& r) { return r.events.size() >= 4 &&
                r.states.back().revision() == 4; }), "events/state missing");
            {
                std::lock_guard<std::mutex> lock(network.received->mutex);
                const auto& events = network.received->events;
                for (int i=0; i<4; ++i) {
                    const auto& e = events.at(i); const auto& observation = e.observation();
                    Check(e.revision() == static_cast<unsigned>(i+1) && e.publisher_instance() == name, "revision");
                    Check(e.header().robot_id() == "robot-test" && e.header().timestamp_unix_ns() > 1000000000 &&
                        e.header().source_instance() == name && e.header().source_node() == "test_alert_source", "header");
                    Check(observation.sequence() == static_cast<unsigned>(i+1) && observation.sample_time_ns() == (i+1)*100 &&
                        observation.clock_domain() == "simulation" && observation.key().session() == "control-1" &&
                        observation.unit() == "rad" && observation.reference_frame() == "mujoco_world", "source metadata");
                }
                Check(events.at(2).observation().transition() == wire::TRANSITION_UNKNOWN &&
                    !events.at(2).observation().valid() && events.at(3).observation().transition() == wire::TRANSITION_CLEARED,
                    "unknown/clear mapping");
            }
            manager.ForgetSession("motion", "control-1");
            Check(network.Wait([](const auto& r) { return r.states.back().revision() == 5 &&
                r.states.back().observations_size() == 0; }), "session removal state missing");
            link.Stop();
            Check(network.Wait([](const auto& r) { return !r.states.back().online(); }), "offline state missing");
            manager.Stop(); Check(link.Stats().publish_failures == 0, "transport failure");
        } else if (name == "late" || name == "gap") {
            auto options = Config(); if (name == "gap") options.history_capacity = 1;
            Manager manager(options); manager.Publish(Make());
            if (name == "gap") { manager.Publish(Make(2, Transition::Reminder)); manager.Publish(Make(3, Transition::Reminder)); }
            LinkPublisher link(manager, network.publisher, LinkConfig("first-instance"));
            // No subscriber yet: wait until the volatile event has been sent.
            const auto deadline = std::chrono::steady_clock::now()+2s;
            while (!link.Stats().events_published && std::chrono::steady_clock::now()<deadline) std::this_thread::sleep_for(5ms);
            Check(link.Stats().events_published > 0, "event never sent");
            network.Subscribe();
            Check(network.Wait([&](const auto& r) { return !r.states.empty() && r.states.back().observations_size() == 1 &&
                r.states.back().observations(0).sequence() == (name == "gap" ? 3 : 1); }), "late subscriber did not recover state");
            {
                std::lock_guard<std::mutex> lock(network.received->mutex);
                const auto& s = network.received->states.back();
                Check(s.stale_after_ms() == 300 && s.online() && s.heartbeat_sequence() > 0, "heartbeat contract");
                if (name == "gap") Check(s.history_gaps() == 1, "history overflow not exposed");
            }
            link.Stop();
            LinkPublisher restarted(manager, network.publisher, LinkConfig("second-instance"));
            Check(network.Wait([](const auto& r) { return r.states.back().publisher_instance() == "second-instance" &&
                r.states.back().online(); }), "restart identity missing");
            restarted.Stop(); manager.Stop();
        } else throw std::runtime_error("unknown case");
        std::cout << "alerts Link " << name << " OK\n";
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
