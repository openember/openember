#include "openember/power/controller.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <unordered_set>

namespace openember::power {
namespace {
bool Id(const std::string& s) {
    if (s.empty() || s.size() > 127) return false;
    return std::all_of(s.begin(), s.end(), [](unsigned char c) {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '.' ||
            c == ':' || c == '/' || c == '@';
    });
}
bool Same(const Request& a, const Request& b) {
    return a.power_id == b.power_id && a.service_session == b.service_session &&
        a.clock_domain == b.clock_domain && a.deadline_monotonic_ns == b.deadline_monotonic_ns &&
        a.target.resource_id == b.target.resource_id && a.target.kind == b.target.kind &&
        a.target.level == b.target.level;
}
bool Kind(ResourceKind k) {
    return k == ResourceKind::kChannel || k == ResourceKind::kFan || k == ResourceKind::kLight;
}
}
Controller::Controller(IPowerDevice& device, std::string session, std::string domain, std::size_t capacity)
    : device_(device), capabilities_(device.GetCapabilities()), session_(std::move(session)),
      clock_domain_(std::move(domain)), capacity_(capacity) {
    if (!Id(session_) || !Id(clock_domain_) || !Id(capabilities_.power_id) || !capacity || capacity > 4096 ||
        capabilities_.resources.size() > 64 || capabilities_.battery_ids.size() > 16)
        throw std::invalid_argument("invalid power controller configuration");
    std::unordered_set<std::string> ids;
    for (const auto& r : capabilities_.resources)
        if (!Id(r.id) || !Kind(r.kind) || !ids.insert(r.id).second ||
            (r.kind == ResourceKind::kChannel && r.variable_level))
            throw std::invalid_argument("invalid power resource capability");
    ids.clear();
    for (const auto& id : capabilities_.battery_ids)
        if (!Id(id) || !ids.insert(id).second) throw std::invalid_argument("invalid battery identity");
}
Result Controller::Handle(const Request& q, const Context& c, std::uint64_t now) {
    const auto reject = [&](Reason reason, const char* message) {
        return Result{q.request_id, Outcome::kRejected, reason, message};
    };
    if (!c.authenticated || !Id(c.principal) || !c.authorize)
        return reject(Reason::kUnauthorized, "resource control is not authorized");
    if (!Id(q.request_id) || !Id(q.power_id) || !Id(q.target.resource_id) || !Kind(q.target.kind) ||
        !std::isfinite(q.target.level) || q.target.level < 0 || q.target.level > 1)
        return reject(Reason::kInvalidRequest, "invalid identity, resource kind or target");
    if (q.service_session != session_ || q.clock_domain != clock_domain_)
        return reject(Reason::kWrongSession, "power session or clock domain mismatch");
    if (q.power_id != capabilities_.power_id)
        return reject(Reason::kUnsupported, "unknown power device");
    // An expired original request can never execute, including after restart.
    if (!now || q.deadline_monotonic_ns <= now)
        return reject(Reason::kExpired, "request deadline elapsed");
    const auto authorize = [c, target=q.target](bool ready, std::uint64_t time) -> GateResult {
        try { return c.authorize(target, ready, time); }
        catch (...) { return {Reason::kNotReady, "authorization authority unavailable"}; }
    };
    // Cached results still require current operation permission, but never
    // need a new readiness permit because no new hardware write takes place.
    const auto permission = authorize(false, now);
    if (!permission.Allowed()) return {q.request_id, Outcome::kRejected, permission.reason, permission.message};
    const Key key{c.principal, q.request_id};
    if (const auto it = history_.find(key); it != history_.end()) {
        if (!Same(it->second.request, q)) return reject(Reason::kConflict, "request ID reused with different content");
        return it->second.result;
    }
    const auto resource = std::find_if(capabilities_.resources.begin(), capabilities_.resources.end(),
        [&](const auto& r) { return r.id == q.target.resource_id; });
    if (resource == capabilities_.resources.end() || !resource->writable || resource->kind != q.target.kind)
        return reject(Reason::kUnsupported, "resource operation is not supported");
    if ((resource->kind == ResourceKind::kChannel || !resource->variable_level) &&
        q.target.level != 0 && q.target.level != 1)
        return reject(Reason::kInvalidRequest, "resource accepts only off/on targets");
    const bool requires_ready = q.target.level > 0 ? resource->enable_requires_ready : resource->disable_requires_ready;
    const auto admission = authorize(requires_ready, now);
    if (!admission.Allowed()) return {q.request_id, Outcome::kRejected, admission.reason, admission.message};
    if (history_.size() >= capacity_) return reject(Reason::kBusy, "session request history is full");
    // Reserve before touching hardware. A thrown backend operation may already
    // have been sent, so retain Unknown and never execute its duplicate again.
    auto inserted = history_.emplace(key, Entry{q, {q.request_id, Outcome::kUnknown,
        Reason::kBackendFailure, "device execution outcome unknown"}}).first;
    try {
        const auto result = device_.ExecuteChecked(q.target, q.deadline_monotonic_ns,
            [authorize, requires_ready, deadline=q.deadline_monotonic_ns](std::uint64_t time) -> GateResult {
                if (time >= deadline) return {Reason::kExpired, "request expired before dispatch"};
                return authorize(requires_ready, time);
            });
        switch (result.outcome) {
        case Outcome::kAccepted: case Outcome::kConfirmed: case Outcome::kUnknown: case Outcome::kRejected:
            inserted->second.result = {q.request_id, result.outcome,
                result.reason != Reason::kNone ? result.reason :
                    (result.outcome == Outcome::kRejected || result.outcome == Outcome::kUnknown ? Reason::kBackendFailure : Reason::kNone),
                result.message.substr(0, 255)};
            break;
        default: break;
        }
    } catch (...) { /* Unknown is intentionally retained. */ }
    return inserted->second.result;
}
} // namespace openember::power
