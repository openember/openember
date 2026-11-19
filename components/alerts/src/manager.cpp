#include "openember/alerts/manager.hpp"
#include <algorithm>
#include <atomic>
#include <cmath>
#include <condition_variable>
#include <deque>
#include <iomanip>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <thread>

namespace openember::alerts {
bool Key::operator==(const Key& o) const {
    return source == o.source && device == o.device && session == o.session && code == o.code;
}
const char* TransitionName(Transition t) {
    switch (t) {
    case Transition::Raised: return "ACTIVE";
    case Transition::Reminder: return "REMINDER";
    case Transition::Cleared: return "CLEARED";
    case Transition::Unknown: return "UNKNOWN";
    }
    return "INVALID";
}
namespace {
bool Text(const std::string& text, bool required = false) {
    return (!required || !text.empty()) && text.size() <= 256 &&
        std::none_of(text.begin(), text.end(), [](unsigned char c) { return c < 32 || c == 127; });
}
bool Valid(const Event& e) {
    return Text(e.key.source, true) && Text(e.key.device, true) && Text(e.key.session, true) &&
        Text(e.key.code, true) && Text(e.clock_domain, true) && Text(e.message) && Text(e.unit) &&
        Text(e.reference_frame) && Text(e.context) && e.sequence > 0 &&
        (e.severity == Severity::Warning || e.severity == Severity::Error) &&
        (e.transition == Transition::Raised || e.transition == Transition::Reminder ||
         e.transition == Transition::Cleared || e.transition == Transition::Unknown) &&
        (e.transition == Transition::Unknown || e.valid) &&
        std::isfinite(e.value) && std::isfinite(e.threshold) && std::isfinite(e.clear_threshold);
}
std::string Format(const Event& e) {
    std::ostringstream out;
    out << std::setprecision(17) << "ALERT source=" << std::quoted(e.key.source)
        << " device=" << std::quoted(e.key.device) << " session=" << std::quoted(e.key.session)
        << " code=" << std::quoted(e.key.code) << " edge=" << TransitionName(e.transition)
        << " sequence=" << e.sequence << " time_ns=" << e.time_ns
        << " duration_ns=" << e.duration_ns << " clock_domain=" << std::quoted(e.clock_domain)
        << " valid=" << e.valid << " value=" << e.value << " threshold=" << e.threshold
        << " clear_threshold=" << e.clear_threshold << " unit=" << std::quoted(e.unit)
        << " reference_frame=" << std::quoted(e.reference_frame) << " context=" << std::quoted(e.context)
        << " message=" << std::quoted(e.message);
    return out.str();
}
logging::LoggerConfig LogConfig(logging::LoggerConfig config) {
    static std::atomic<std::uint64_t> ids{0};
    // Dedicated instance: ordinary process log filters cannot suppress warnings.
    config.make_default = false;
    config.name += "-" + std::to_string(++ids);
    config.log_level = config.flush_level = config.topic_level = "warn";
    return config;
}
} // namespace
struct Manager::Impl {
    Options options;
    logging::Logger logger;
    mutable std::mutex mutex;
    std::mutex stop_mutex;
    std::condition_variable ready;
    std::deque<Event> queue;
    std::deque<Notification> history;
    std::uint64_t revision = 0;
    std::uint64_t evicted_revision = 0;
    std::vector<Event> states;
    Statistics stats;
    bool stopping = false;
    std::thread worker;
    explicit Impl(Options value) : options(std::move(value)), logger(LogConfig(options.logging)) {
        if (!options.queue_capacity || options.queue_capacity > 65536 ||
            !options.state_capacity || options.state_capacity > 65536 ||
            !options.history_capacity || options.history_capacity > 65536)
            throw std::invalid_argument("alert capacities must be in [1, 65536]");
        if (!logger.available()) throw std::runtime_error("alert logger initialization failed");
        states.reserve(options.state_capacity);
        worker = std::thread([this] { Dispatch(); });
    }
    void Dispatch() {
        std::uint64_t reported_rejections = 0;
        for (;;) {
            Event event;
            bool have_event = false;
            std::uint64_t rejections;
            {
                std::unique_lock<std::mutex> lock(mutex);
                ready.wait(lock, [&] { return stopping || !queue.empty() || stats.rejected != reported_rejections; });
                rejections = stats.rejected;
                if (!queue.empty()) {
                    event = std::move(queue.front()); queue.pop_front(); have_event = true;
                } else if (stopping && rejections == reported_rejections) break;
            }
            std::uint64_t errors = 0;
            if (rejections != reported_rejections) {
                try { logger.warn_tag("alerts", "rejected_events=" + std::to_string(rejections)); }
                catch (...) { ++errors; }
                reported_rejections = rejections;
            }
            if (have_event) {
                try { logger.write(event.severity == Severity::Error ? logging::Level::Error : logging::Level::Warn,
                                   "alerts", Format(event)); }
                catch (...) { ++errors; }
                try { if (options.observer) options.observer(event); }
                catch (...) { ++errors; }
            }
            std::lock_guard<std::mutex> lock(mutex);
            stats.dispatch_errors += errors;
            if (have_event) ++stats.delivered;
        }
    }
};
Manager::Manager(Options options) : impl_(std::make_unique<Impl>(std::move(options))) {}
Manager::~Manager() { Stop(); }
PublishResult Manager::Publish(Event event) {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    const auto reject = [&](PublishResult result) {
        ++impl_->stats.rejected; impl_->ready.notify_one(); return result;
    };
    if (impl_->stopping) return reject(PublishResult::Stopped);
    if (!Valid(event)) return reject(PublishResult::Invalid);
    const auto entry = std::find_if(impl_->states.begin(), impl_->states.end(),
                                  [&](const Event& old) { return old.key == event.key; });
    if (entry != impl_->states.end() && (event.clock_domain != entry->clock_domain ||
        event.sequence <= entry->sequence || event.time_ns < entry->time_ns)) return reject(PublishResult::Stale);
    if (impl_->queue.size() == impl_->options.queue_capacity ||
        (entry == impl_->states.end() && impl_->states.size() == impl_->options.state_capacity))
        return reject(PublishResult::Full);
    impl_->queue.push_back(event);
    impl_->history.push_back({++impl_->revision, event});
    if (impl_->history.size() > impl_->options.history_capacity) {
        impl_->evicted_revision = impl_->history.front().revision;
        impl_->history.pop_front();
    }
    if (entry == impl_->states.end()) impl_->states.push_back(std::move(event));
    else *entry = std::move(event);
    ++impl_->stats.accepted;
    impl_->ready.notify_one();
    return PublishResult::Accepted;
}
std::vector<Event> Manager::Snapshot() const {
    std::lock_guard<std::mutex> lock(impl_->mutex); return impl_->states;
}
Statistics Manager::Stats() const {
    std::lock_guard<std::mutex> lock(impl_->mutex); return impl_->stats;
}
UpdateBatch Manager::ReadSince(std::uint64_t after_revision) const {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    if (after_revision > impl_->revision) throw std::invalid_argument("future alert history cursor");
    UpdateBatch batch;
    batch.revision = impl_->revision; batch.statistics = impl_->stats;
    batch.states = impl_->states;
    // A gap is detectable even after old events have left the bounded history.
    if (after_revision < impl_->evicted_revision)
        batch.history_gap = true;
    for (const auto& event : impl_->history)
        if (event.revision > after_revision) batch.events.push_back(event);
    return batch;
}
void Manager::ForgetSession(const std::string& source, const std::string& session) {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    auto& states = impl_->states;
    const auto before = states.size();
    states.erase(std::remove_if(states.begin(), states.end(), [&](const Event& e) {
        return e.key.source == source && e.key.session == session;
    }), states.end());
    if (states.size() != before) ++impl_->revision;
}
void Manager::Stop() {
    std::lock_guard<std::mutex> stop_lock(impl_->stop_mutex);
    {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        impl_->stopping = true; impl_->ready.notify_one();
    }
    if (impl_->worker.joinable()) impl_->worker.join();
}
} // namespace openember::alerts
