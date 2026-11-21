#include "openember/power/authority.hpp"
#include <algorithm>
#include <map>
#include <mutex>
#include <stdexcept>
namespace openember::power {
namespace {
constexpr std::uint64_t MaxPermitNs = 1000000000ULL;
bool Id(const std::string& s) {
    return !s.empty() && s.size() <= 127 && std::all_of(s.begin(), s.end(), [](unsigned char c) {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
            c == '_' || c == '-' || c == '.' || c == ':' || c == '/' || c == '@';
    });
}
bool Valid(const Permission& p) {
    return Id(p.resource_id) && (p.operation == Operation::kSetLevel || p.operation == Operation::kEnable || p.operation == Operation::kDisable);
}
bool Allows(const std::vector<Permission>& grants, const Target& target) {
    const auto operation = target.kind == ResourceKind::kChannel ?
        (target.level > 0 ? Operation::kEnable : Operation::kDisable) : Operation::kSetLevel;
    return std::any_of(grants.begin(), grants.end(), [&](const auto& p) {
        return p.resource_id == target.resource_id && p.operation == operation;
    });
}
}
struct Authority::State {
    std::mutex mutex;
    std::string robot, power, session, domain, issuer;
    bool simulation = false, active = true;
    std::uint64_t last_sequence = 0;
    std::optional<ReadinessPermit> permit;
    std::map<std::string, std::vector<Permission>> permissions;
};
Authority::Authority(std::string robot, std::string power, std::string session, std::string domain, bool simulation)
    : state_(std::make_shared<State>()) {
    if (!Id(robot) || !Id(power) || !Id(session) || !Id(domain)) throw std::invalid_argument("invalid power authority identity");
    state_->robot = std::move(robot); state_->power = std::move(power);
    state_->session = std::move(session); state_->domain = std::move(domain); state_->simulation = simulation;
}
Authority::~Authority() { Close(); }
void Authority::Close() {
    std::lock_guard<std::mutex> lock(state_->mutex);
    state_->active = false; state_->permit.reset();
}
void Authority::SetPermissions(const std::string& principal, std::vector<Permission> grants) {
    if (!Id(principal) || grants.size() > 192 || !std::all_of(grants.begin(), grants.end(), Valid))
        throw std::invalid_argument("invalid power permissions");
    std::lock_guard<std::mutex> lock(state_->mutex);
    if (grants.empty()) state_->permissions.erase(principal);
    else state_->permissions[principal] = std::move(grants);
}
void Authority::BindTrustedIssuer(const std::string& session) {
    if (!Id(session)) throw std::invalid_argument("invalid readiness issuer session");
    std::lock_guard<std::mutex> lock(state_->mutex);
    if (state_->issuer != session) {
        state_->issuer = session; state_->last_sequence = 0; state_->permit.reset();
    }
}
bool Authority::ApplyTrustedPermit(ReadinessPermit p, std::uint64_t now) {
    std::lock_guard<std::mutex> lock(state_->mutex);
    if (!state_->active || state_->issuer.empty() || p.issuer_session != state_->issuer ||
        p.robot_id != state_->robot || p.power_id != state_->power || p.power_session != state_->session ||
        p.clock_domain != state_->domain || p.simulation != state_->simulation ||
        !now || !p.sequence || p.sequence <= state_->last_sequence || !p.issued_monotonic_ns ||
        p.issued_monotonic_ns > now || p.expires_monotonic_ns <= now ||
        p.expires_monotonic_ns - p.issued_monotonic_ns > MaxPermitNs ||
        p.operations.size() > 192 || !std::all_of(p.operations.begin(), p.operations.end(), Valid)) return false;
    state_->last_sequence = p.sequence; state_->permit = std::move(p); return true;
}
void Authority::RevokeReadiness() { std::lock_guard<std::mutex> lock(state_->mutex); state_->permit.reset(); }
Context Authority::ForAuthenticated(const std::string& principal) const {
    if (!Id(principal)) throw std::invalid_argument("invalid authenticated principal");
    auto s = state_;
    return {principal, true, [s, principal](const Target& target, bool ready, std::uint64_t now) -> GateResult {
        std::lock_guard<std::mutex> lock(s->mutex);
        const auto grants = s->permissions.find(principal);
        if (!s->active || grants == s->permissions.end() || !Allows(grants->second, target))
            return {Reason::kUnauthorized, "resource operation is not authorized"};
        if (!ready) return {};
        if (!s->permit || !now || now < s->permit->issued_monotonic_ns || now >= s->permit->expires_monotonic_ns ||
            !Allows(s->permit->operations, target))
            return {Reason::kNotReady, "fresh resource readiness permit required"};
        return {};
    }};
}
}
