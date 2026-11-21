#pragma once
#include <map>
#include "openember/power/power.hpp"

namespace openember::power {
struct Request {
    std::string request_id, power_id, service_session, clock_domain;
    std::uint64_t deadline_monotonic_ns = 0;
    Target target;
};
struct Result {
    std::string request_id;
    Outcome outcome = Outcome::kRejected;
    Reason reason = Reason::kNone;
    std::string message;
};
// Constructed only by a trusted authenticated adapter. authorize is local,
// thread-safe, nonblocking and re-evaluates live policy/readiness at dispatch.
struct Context {
    std::string principal;
    bool authenticated = false;
    std::function<GateResult(const Target&, bool requires_ready, std::uint64_t now)> authorize;
};

// Single controller executor; not a concurrent RPC handler. Device operations may block.
// No retries, leases, automatic power-on, or automatic restoration of intent.
// Executed request IDs remain reserved for this session, even after expiry.
// Capacity exhaustion fails closed; a new controller needs a NEW service session.
class Controller {
public:
    Controller(IPowerDevice& device, std::string session, std::string clock_domain,
               std::size_t capacity = 256);
    Controller(const Controller&) = delete;
    Controller& operator=(const Controller&) = delete;
    Result Handle(const Request&, const Context&, std::uint64_t now_monotonic_ns);
    const Capabilities& GetCapabilities() const { return capabilities_; }
    const std::string& Session() const { return session_; }
    const std::string& ClockDomain() const { return clock_domain_; }
    std::size_t Capacity() const { return capacity_; }
private:
    struct Entry { Request request; Result result; };
    using Key = std::pair<std::string, std::string>; // principal + request ID
    IPowerDevice& device_;
    const Capabilities capabilities_;
    std::string session_, clock_domain_;
    std::size_t capacity_;
    std::map<Key, Entry> history_;
};
} // namespace openember::power
