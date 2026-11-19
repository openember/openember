#pragma once
#include "openember/alerts/manager.hpp"
#include "openember/node.hpp"
#include <chrono>
#include <memory>

namespace openember::alerts {
inline constexpr const char* kEventsTopic = "/alerts/events";
inline constexpr const char* kStateTopic = "/alerts/state";
struct LinkOptions {
    std::string robot_id;
    // Empty generates a fresh id. Reuse is forbidden after restart/replacement.
    std::string publisher_instance;
    std::chrono::milliseconds poll_interval{20}, state_interval{1000};
    std::uint32_t stale_after_ms = 3000;
};
struct LinkStatistics {
    std::uint64_t events_published = 0, states_published = 0;
    std::uint64_t publish_failures = 0, history_gaps = 0;
};
// Independent journal reader: no logging sink or log dispatcher dependency.
// Manager and live Node/Context must outlive this adapter; stop producers first,
// then Stop() here, then stop Manager and finally shut down the Context.
class LinkPublisher {
public:
    LinkPublisher(Manager& manager, std::shared_ptr<Node> node, LinkOptions options);
    ~LinkPublisher();
    LinkPublisher(const LinkPublisher&) = delete;
    LinkPublisher& operator=(const LinkPublisher&) = delete;
    void Stop();
    LinkStatistics Stats() const;
    const std::string& Instance() const;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace openember::alerts
