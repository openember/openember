#pragma once
#include "openember/logging/logger.hpp"
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace openember::alerts {
enum class Severity { Warning, Error };
enum class Transition { Raised, Reminder, Cleared, Unknown };
struct Key {
    std::string source, device, session, code;
    bool operator==(const Key& other) const;
};
struct Event {
    Key key;
    Severity severity = Severity::Warning;
    Transition transition = Transition::Raised;
    std::uint64_t sequence = 0, time_ns = 0, duration_ns = 0;
    std::string clock_domain, message, unit, reference_frame, context;
    double value = 0, threshold = 0, clear_threshold = 0;
    bool valid = true;
};
struct Options {
    std::size_t queue_capacity = 128, state_capacity = 128;
    logging::LoggerConfig logging = logging::LoggerConfig::from_build_defaults("openember-alerts");
    // Called on the dispatch thread, after logging. Must return in bounded time;
    // must not call Stop() or destroy this manager. Exceptions are counted.
    std::function<void(const Event&)> observer;
};
enum class PublishResult { Accepted, Invalid, Stale, Full, Stopped };
struct Statistics {
    std::uint64_t accepted = 0, rejected = 0, delivered = 0, dispatch_errors = 0;
};
// Bounded in-process state/event system. Publish never performs sink I/O; it
// takes a short mutex and copies bounded metadata. This is not a hard RT API.
class Manager {
public:
    explicit Manager(Options options = {});
    ~Manager();
    Manager(const Manager&) = delete;
    Manager& operator=(const Manager&) = delete;
    PublishResult Publish(Event event);
    std::vector<Event> Snapshot() const;
    Statistics Stats() const;
    // Call after stopping a producer, before starting its next session. Pending
    // events still drain to logging; removal does not fabricate a clear event.
    void ForgetSession(const std::string& source, const std::string& session);
    // Stop acceptance, drain accepted events and join. Call outside control loops.
    void Stop();
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
const char* TransitionName(Transition transition);
} // namespace openember::alerts
