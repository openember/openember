#include "openember/alerts/link_publisher.hpp"
#include "openember/msgs/alerts/v1/alerts.pb.h"
#include <csignal>
#include <iostream>
#include <map>
#include <mutex>
#include <thread>
namespace wire = openember::msgs::alerts::v1;
using namespace std::chrono_literals;
namespace {
volatile std::sig_atomic_t stopped = 0;
void Signal(int) { stopped = 1; }
struct Freshness {
    std::uint64_t heartbeat = 0;
    std::chrono::steady_clock::time_point received;
    std::uint32_t stale_after_ms = 0;
    bool online = false, reported_stale = false;
};
}
int main(int argc, char** argv) {
    try {
        openember::RuntimeOptions runtime;
        int seconds = 0;
        for (int i=1; i<argc; ++i) {
            const std::string option = argv[i];
            if (option == "--help") {
                std::cout << "openember_alert_monitor --robot ID [--connect ENDPOINT] [--namespace NAME] [--seconds N]\n";
                return 0;
            }
            if (i+1 >= argc) throw std::invalid_argument("missing argument value");
            const std::string value = argv[++i];
            if (option == "--robot") runtime.robot_id = value;
            else if (option == "--connect") runtime.link.connect = value;
            else if (option == "--namespace") runtime.namespace_name = value;
            else if (option == "--seconds") {
                std::size_t used = 0; seconds = std::stoi(value, &used);
                if (used != value.size() || seconds < 1 || seconds > 86400) throw std::invalid_argument("invalid seconds");
            } else throw std::invalid_argument("unknown option: " + option);
        }
        std::signal(SIGINT, Signal); std::signal(SIGTERM, Signal);
        auto context = std::make_shared<openember::Context>(runtime);
        if (!context->Init()) throw std::runtime_error("Link initialization failed");
        std::mutex mutex;
        std::map<std::string, Freshness> publishers;
        // Callback state outlives the Node and its subscriptions.
        auto node = std::make_shared<openember::Node>(context, "alert_monitor");
        node->CreateSubscriber<wire::AlertEvent>(openember::alerts::kEventsTopic, [&](const auto& event) {
            std::lock_guard<std::mutex> lock(mutex);
            std::cout << "ALERT_EVENT " << event.ShortDebugString() << std::endl;
        });
        node->CreateSubscriber<wire::AlertState>(openember::alerts::kStateTopic, [&](const auto& state) {
            std::lock_guard<std::mutex> lock(mutex);
            if (state.publisher_instance().empty() || state.stale_after_ms() == 0) return;
            if (!publishers.count(state.publisher_instance()) && publishers.size() >= 256) return;
            auto& fresh = publishers[state.publisher_instance()];
            if (state.heartbeat_sequence() <= fresh.heartbeat) return;
            fresh = {state.heartbeat_sequence(), std::chrono::steady_clock::now(), state.stale_after_ms(), state.online(), false};
            int active = 0, unknown = 0;
            for (const auto& observation : state.observations()) {
                active += observation.transition() == wire::TRANSITION_RAISED || observation.transition() == wire::TRANSITION_REMINDER;
                unknown += observation.transition() == wire::TRANSITION_UNKNOWN || observation.transition() == wire::TRANSITION_UNSPECIFIED;
            }
            std::cout << "ALERT_STATE publisher=" << state.publisher_instance() << " revision=" << state.revision()
                << " heartbeat=" << state.heartbeat_sequence() << " online=" << state.online()
                << " conditions=" << state.observations_size() << " active=" << active << " unknown=" << unknown
                << " history_gaps=" << state.history_gaps() << " publish_failures=" << state.publish_failures() << std::endl;
        });
        const auto started = std::chrono::steady_clock::now();
        std::cout << "ALERT_MONITOR robot=" << runtime.robot_id << " events=/alerts/events state=/alerts/state" << std::endl;
        while (!stopped && (!seconds || std::chrono::steady_clock::now()-started < std::chrono::seconds(seconds))) {
            {
                std::lock_guard<std::mutex> lock(mutex);
                for (auto& entry : publishers) {
                    auto& fresh = entry.second;
                    if (fresh.online && !fresh.reported_stale && std::chrono::steady_clock::now()-fresh.received >
                        std::chrono::milliseconds(fresh.stale_after_ms)) {
                        fresh.reported_stale = true;
                        std::cout << "ALERT_STALE publisher=" << entry.first << " state=unknown" << std::endl;
                    }
                }
            }
            std::this_thread::sleep_for(50ms);
        }
        node.reset(); context->Shutdown();
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
