#include "openember/alerts/link_publisher.hpp"
#include "openember/msgs/alerts/v1/alerts.pb.h"
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <random>
#include <stdexcept>
#include <thread>

namespace openember::alerts {
namespace {
namespace wire = openember::msgs::alerts::v1;
std::uint64_t UnixNs() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}
bool Identifier(const std::string& value) {
    if (value.empty() || value.size() > 256) return false;
    for (unsigned char c : value) if (c < 33 || c == 127) return false;
    return true;
}
wire::AlertObservation Observation(const Event& event) {
    wire::AlertObservation out;
    auto* key = out.mutable_key();
    key->set_source(event.key.source); key->set_device(event.key.device);
    key->set_session(event.key.session); key->set_code(event.key.code);
    out.set_severity(event.severity == Severity::Error ? wire::SEVERITY_ERROR : wire::SEVERITY_WARNING);
    switch (event.transition) {
    case Transition::Raised: out.set_transition(wire::TRANSITION_RAISED); break;
    case Transition::Reminder: out.set_transition(wire::TRANSITION_REMINDER); break;
    case Transition::Cleared: out.set_transition(wire::TRANSITION_CLEARED); break;
    case Transition::Unknown: out.set_transition(wire::TRANSITION_UNKNOWN); break;
    }
    out.set_sequence(event.sequence); out.set_sample_time_ns(event.time_ns);
    out.set_clock_domain(event.clock_domain); out.set_duration_ns(event.duration_ns);
    out.set_valid(event.valid); out.set_value(event.value); out.set_threshold(event.threshold);
    out.set_clear_threshold(event.clear_threshold); out.set_unit(event.unit);
    out.set_reference_frame(event.reference_frame); out.set_context(event.context); out.set_message(event.message);
    return out;
}
}
struct LinkPublisher::Impl {
    Manager& manager;
    std::shared_ptr<Node> node;
    LinkOptions options;
    Publisher<wire::AlertEvent> events;
    Publisher<wire::AlertState> state;
    std::atomic<std::uint64_t> event_count{0}, state_count{0}, failures{0}, gaps{0};
    std::uint64_t cursor = 0, heartbeat = 0;
    bool first = true;
    std::mutex mutex, stop_mutex;
    std::condition_variable wake;
    bool stopping = false;
    std::thread worker;
    Impl(Manager& m, std::shared_ptr<Node> n, LinkOptions o)
        : manager(m), node(std::move(n)), options(std::move(o)) {
        if (!node || !Identifier(options.robot_id) || options.poll_interval.count() < 1 ||
            options.poll_interval.count() > 1000 || options.state_interval.count() < 50 ||
            options.state_interval.count() > 60000 || options.poll_interval > options.state_interval ||
            options.stale_after_ms < 3 * options.state_interval.count())
            throw std::invalid_argument("invalid alert Link publisher options");
        if (options.publisher_instance.empty()) {
            std::random_device random;
            options.publisher_instance = "alerts-" + std::to_string(UnixNs()) + "-" +
                std::to_string(random()) + "-" + std::to_string(random());
        }
        if (!Identifier(options.publisher_instance)) throw std::invalid_argument("invalid alert publisher instance");
        events = node->CreatePublisher<wire::AlertEvent>(kEventsTopic);
        state = node->CreatePublisher<wire::AlertState>(kStateTopic);
        worker = std::thread([this] { Run(); });
    }
    void Header(openember::msgs::common::v1::Header* h, std::uint64_t sequence) {
        h->set_source_node(node->Name()); h->set_source_instance(options.publisher_instance);
        h->set_robot_id(options.robot_id); h->set_namespace_name(node->Namespace());
        h->set_sequence(sequence); h->set_timestamp_unix_ns(UnixNs());
    }
    void SendState(const UpdateBatch& batch, bool online) {
        wire::AlertState out;
        Header(out.mutable_header(), ++heartbeat);
        out.set_publisher_instance(options.publisher_instance); out.set_revision(batch.revision);
        for (const auto& event : batch.states) *out.add_observations() = Observation(event);
        out.set_heartbeat_sequence(heartbeat); out.set_stale_after_ms(options.stale_after_ms);
        out.set_rejected_events(batch.statistics.rejected); out.set_history_gaps(gaps.load());
        out.set_publish_failures(failures.load()); out.set_online(online);
        try { if (state.Publish(out)) ++state_count; else ++failures; }
        catch (...) { ++failures; }
    }
    void Tick(bool online, bool periodic) {
        const auto batch = manager.ReadSince(cursor);
        if (batch.history_gap) ++gaps;
        for (const auto& notification : batch.events) {
            wire::AlertEvent out;
            Header(out.mutable_header(), notification.revision);
            out.set_publisher_instance(options.publisher_instance); out.set_revision(notification.revision);
            *out.mutable_observation() = Observation(notification.event);
            try { if (events.Publish(out)) ++event_count; else ++failures; }
            catch (...) { ++failures; }
        }
        if (first || periodic || !online || batch.revision != cursor) SendState(batch, online);
        cursor = batch.revision; first = false;
    }
    void Run() {
        auto next_state = std::chrono::steady_clock::now();
        for (;;) {
            bool done;
            { std::lock_guard<std::mutex> lock(mutex); done = stopping; }
            const auto now = std::chrono::steady_clock::now();
            try { Tick(!done, now >= next_state); }
            catch (...) { ++failures; }
            if (done) return;
            if (now >= next_state) next_state = now + options.state_interval;
            std::unique_lock<std::mutex> lock(mutex);
            wake.wait_for(lock, options.poll_interval, [&] { return stopping; });
        }
    }
};
LinkPublisher::LinkPublisher(Manager& manager, std::shared_ptr<Node> node, LinkOptions options)
    : impl_(std::make_unique<Impl>(manager, std::move(node), std::move(options))) {}
LinkPublisher::~LinkPublisher() { Stop(); }
void LinkPublisher::Stop() {
    std::lock_guard<std::mutex> stop_lock(impl_->stop_mutex);
    { std::lock_guard<std::mutex> lock(impl_->mutex); impl_->stopping = true; impl_->wake.notify_all(); }
    if (impl_->worker.joinable()) impl_->worker.join();
}
LinkStatistics LinkPublisher::Stats() const {
    return {impl_->event_count.load(), impl_->state_count.load(), impl_->failures.load(), impl_->gaps.load()};
}
const std::string& LinkPublisher::Instance() const { return impl_->options.publisher_instance; }
} // namespace openember::alerts
