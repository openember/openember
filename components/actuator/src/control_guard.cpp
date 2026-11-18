#include "openember/actuator/control_guard.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <set>
#include <stdexcept>
#include <tuple>

namespace openember::actuator::control {
namespace {
constexpr std::size_t kMaxJoints = 32, kMaxRequests = 128, kMaxReplayStreams = 128;
bool Id(const std::string& value, std::size_t maximum = 63) {
    return !value.empty() && value.size() <= maximum &&
        std::all_of(value.begin(), value.end(), [](unsigned char c) { return c > 0 && c < 128; });
}
bool Mode(JointControlMode mode) {
    switch (mode) {
        case JointControlMode::kDisabled: case JointControlMode::kPosition:
        case JointControlMode::kVelocity: case JointControlMode::kEffort:
        case JointControlMode::kMit: return true;
        default: return false;
    }
}
bool Same(const Request& a, const Request& b) {
    const auto context = [](const Context& c) {
        return std::tie(c.robot_id, c.controller_id, c.controller_session, c.model_revision,
                        c.producer_session, c.sequence, c.clock_domain, c.issued_time_ns, c.expires_time_ns);
    };
    return context(a.context) == context(b.context) && a.request_id == b.request_id &&
        a.operation == b.operation && a.lease.generation == b.lease.generation &&
        a.lease.token == b.lease.token && a.requested_lease_duration_ns == b.requested_lease_duration_ns;
}
bool AddOverflows(std::uint64_t now, std::uint64_t duration) {
    return duration > std::numeric_limits<std::uint64_t>::max() - now;
}
bool Safety(Operation op) {
    return op == Operation::kDisarm || op == Operation::kEmergencyStop || op == Operation::kClearFault;
}
}  // namespace

Decision ValidateCapabilities(const Capabilities& c) {
    if (c.contract_version != 2 || !Id(c.robot_id) || !Id(c.controller_id) || !Id(c.controller_session) ||
        !Id(c.model_revision, 127) || !Id(c.clock_domain, 127) ||
        c.clock_domain.compare(0, 15, "host-monotonic:") != 0 || c.clock_domain.size() <= 15 ||
        !c.max_command_duration_ns || !c.max_lease_duration_ns || !c.command_timeout_ns ||
        c.joints.empty() || c.joints.size() > kMaxJoints) return {Reason::kInvalidArgument};
    std::set<std::string> names;
    for (const auto& j : c.joints) {
        if (!Id(j.joint_name) || !names.insert(j.joint_name).second || !j.max_feedback_age_ns ||
            !std::isfinite(j.min_position_rad) || !std::isfinite(j.max_position_rad) ||
            j.min_position_rad > j.max_position_rad || !std::isfinite(j.max_velocity_radps) ||
            j.max_velocity_radps <= 0 || !std::isfinite(j.max_effort_nm) || j.max_effort_nm <= 0 ||
            !std::isfinite(j.max_stiffness_nm_per_rad) || j.max_stiffness_nm_per_rad < 0 ||
            !std::isfinite(j.max_damping_nms_per_rad) || j.max_damping_nms_per_rad < 0 ||
            j.supported_modes.empty() || j.supported_modes.size() > 5) return {Reason::kInvalidArgument};
        std::set<JointControlMode> modes;
        for (auto mode : j.supported_modes)
            if (!Mode(mode) || !modes.insert(mode).second) return {Reason::kInvalidArgument};
    }
    return {};
}

Decision ValidateTargets(const Capabilities& c, const std::vector<Target>& targets) {
    if (targets.size() != c.joints.size() || targets.empty() || targets.size() > kMaxJoints)
        return {Reason::kTopologyMismatch};
    std::set<std::string> seen;
    for (const auto& t : targets) {
        const auto found = std::find_if(c.joints.begin(), c.joints.end(), [&](const auto& j) {
            return j.joint_name == t.joint_name;
        });
        if (!Id(t.joint_name) || !seen.insert(t.joint_name).second || found == c.joints.end())
            return {Reason::kTopologyMismatch};
        const auto& j = *found;
        if (!Mode(t.mode) || std::find(j.supported_modes.begin(), j.supported_modes.end(), t.mode) ==
                j.supported_modes.end()) return {Reason::kUnsupportedMode};
        for (double value : {t.position_rad, t.velocity_radps, t.effort_nm,
                            t.stiffness_nm_per_rad, t.damping_nms_per_rad, t.max_effort_nm})
            if (!std::isfinite(value)) return {Reason::kInvalidArgument};
        const bool position = t.mode == JointControlMode::kMit || t.mode == JointControlMode::kPosition;
        const bool velocity = t.mode == JointControlMode::kMit || t.mode == JointControlMode::kVelocity;
        const bool effort = t.mode == JointControlMode::kMit || t.mode == JointControlMode::kEffort;
        if ((!position && t.position_rad != 0) || (!velocity && t.velocity_radps != 0) ||
            (!effort && t.effort_nm != 0) || (t.mode != JointControlMode::kMit &&
                (t.stiffness_nm_per_rad != 0 || t.damping_nms_per_rad != 0)))
            return {Reason::kInvalidArgument};
        if (t.mode == JointControlMode::kDisabled) {
            if (t.max_effort_nm != 0) return {Reason::kInvalidArgument};
            continue;
        }
        if (t.max_effort_nm <= 0 || t.max_effort_nm > j.max_effort_nm ||
            std::abs(t.effort_nm) > t.max_effort_nm || std::abs(t.velocity_radps) > j.max_velocity_radps ||
            t.stiffness_nm_per_rad < 0 || t.stiffness_nm_per_rad > j.max_stiffness_nm_per_rad ||
            t.damping_nms_per_rad < 0 || t.damping_nms_per_rad > j.max_damping_nms_per_rad ||
            (position && !j.continuous && (t.position_rad < j.min_position_rad ||
                                         t.position_rad > j.max_position_rad)))
            return {Reason::kLimitViolation};
    }
    return {};
}

bool SampleFresh(std::uint64_t sequence, std::uint64_t received, bool fresh,
                 std::uint64_t max_age, std::uint64_t now) noexcept {
    return sequence && received && fresh && max_age && received <= now && now - received <= max_age;
}

Guard::Guard(Capabilities c, Backend& backend, TokenFactory tokens)
    : capabilities_(std::move(c)), backend_(backend), tokens_(std::move(tokens)) {
    if (!ValidateCapabilities(capabilities_).Ok() || !tokens_)
        throw std::invalid_argument("invalid actuator v2 capabilities or token factory");
    if (!Disable()) { status_.state = State::kFault; status_.fault_reason = Reason::kBackendFailure; }
}
Guard::~Guard() { Stop(); }

Decision Guard::CheckContext(const Context& c, std::uint64_t now) const {
    if (!now || now < last_now_ns_) return {Reason::kExpired};
    if (!Id(c.robot_id) || !Id(c.controller_id) || !Id(c.controller_session) ||
        !Id(c.model_revision, 127) || !Id(c.producer_session) || !Id(c.clock_domain, 127) || !c.sequence)
        return {Reason::kInvalidArgument};
    if (c.robot_id != capabilities_.robot_id || c.controller_id != capabilities_.controller_id ||
        c.controller_session != capabilities_.controller_session || c.model_revision != capabilities_.model_revision)
        return {Reason::kWrongContext};
    if (c.clock_domain != capabilities_.clock_domain) return {Reason::kClockMismatch};
    if (!c.issued_time_ns || c.issued_time_ns > now || c.expires_time_ns <= c.issued_time_ns ||
        now >= c.expires_time_ns ||
        c.expires_time_ns - c.issued_time_ns > capabilities_.max_command_duration_ns)
        return {Reason::kExpired};
    return {};
}

bool Guard::Owns(const Context& c, const Lease& lease) const {
    return lease_.generation && lease.generation == lease_.generation && Id(lease.token) &&
        lease.token == lease_.token && c.producer_session == status_.owner_producer_session;
}

void Guard::Revoke() noexcept {
    lease_ = {};
    status_.owner_generation = 0;
    status_.owner_producer_session.clear();
    status_.lease_expires_time_ns = 0;
    status_.last_accepted_command_sequence = status_.last_applied_command_sequence = 0;
    status_.last_accepted_command_time_ns = 0;
    management_sequence_ = arm_time_ns_ = 0;
    target_.reset();
}

void Guard::Fault(Reason reason) noexcept {
    Revoke();
    Disable();
    status_.fault_reason = reason;
    fault_time_ns_ = last_now_ns_;
    if (status_.state != State::kStopped && status_.state != State::kEmergencyStop)
        status_.state = State::kFault;
}

void Guard::Stop() noexcept {
    Revoke();
    Disable();
    status_.state = State::kStopped;
}

bool Guard::Disable() noexcept {
    const bool ok = backend_.Disable();
    if (!ok) status_.backend_failure = true;
    return ok;
}

void Guard::Tick(std::uint64_t now) {
    if (!now || now < last_now_ns_) { Fault(Reason::kClockMismatch); return; }
    last_now_ns_ = now;
    if (status_.state == State::kStopped) return;
    if (lease_.generation && now >= status_.lease_expires_time_ns) { Fault(Reason::kExpired); return; }
    if (status_.state == State::kArmed &&
        ((target_ && now >= target_->context.expires_time_ns) ||
         now - (status_.last_accepted_command_time_ns ? status_.last_accepted_command_time_ns : arm_time_ns_)
            >= capabilities_.command_timeout_ns)) Fault(Reason::kExpired);
}

void Guard::TripFault(Reason reason, std::uint64_t now) {
    Tick(now);
    Fault(reason == Reason::kNone ? Reason::kBackendFailure : reason);
}

Decision Guard::Reject(Reason reason) {
    if (status_.rejected_command_count != std::numeric_limits<std::uint64_t>::max())
        ++status_.rejected_command_count;
    status_.last_rejection_reason = reason;
    return {reason};
}

Decision Guard::Accept(const Command& command, std::uint64_t now) {
    Tick(now);
    if (const auto checked = CheckContext(command.context, now); !checked.Ok()) return Reject(checked.reason);
    if (status_.state != State::kArmed || status_.paused) return Reject(Reason::kInvalidState);
    if (!Owns(command.context, command.lease)) return Reject(Reason::kNotOwner);
    if (command.context.sequence <= status_.last_accepted_command_sequence) return Reject(Reason::kReplay);
    if (const auto checked = ValidateTargets(capabilities_, command.joints); !checked.Ok()) return Reject(checked.reason);
    target_ = command;
    status_.last_accepted_command_sequence = command.context.sequence;
    status_.last_accepted_command_time_ns = now;
    return {};
}

Decision Guard::RejectInput(Reason reason, std::uint64_t now) {
    Tick(now);
    return Reject(reason == Reason::kNone ? Reason::kInvalidArgument : reason);
}

Decision Guard::Apply(std::uint64_t now) {
    Tick(now);
    if (status_.state != State::kArmed || status_.paused || !target_) return {Reason::kInvalidState};
    if (!Owns(target_->context, target_->lease) || !CheckContext(target_->context, now).Ok()) {
        Fault(Reason::kExpired); return {Reason::kExpired};
    }
    if (!backend_.Ready() || !backend_.Apply(target_->joints)) {
        status_.backend_failure = true; Fault(); return {Reason::kBackendFailure};
    }
    status_.last_applied_command_sequence = target_->context.sequence;
    return {};
}

ControlResult Guard::Control(const Request& r, std::uint64_t now, Permission permission) {
    Tick(now);
    const auto rejected = [&](Reason reason) {
        return ControlResult{ResultCode::kRejected, reason, status_.state, {}, 0};
    };
    if (const auto checked = CheckContext(r.context, now); !checked.Ok()) return rejected(checked.reason);
    if (status_.state == State::kStopped) return rejected(Reason::kInvalidState);
    if (!Id(r.request_id) || r.operation < Operation::kAcquire || r.operation > Operation::kClearFault)
        return rejected(Reason::kInvalidArgument);
    const bool authorized = r.operation == Operation::kClearFault ? permission.clear_fault :
        (Safety(r.operation) ? permission.stop : permission.control);
    if (!authorized) return rejected(Reason::kNotOwner);
    const Key key{r.context.producer_session, r.request_id};
    for (auto it = cache_.begin(); it != cache_.end();) {
        if (it->second.request.context.expires_time_ns <= now) it = cache_.erase(it);
        else ++it;
    }
    if (const auto found = cache_.find(key); found != cache_.end())
        return Same(r, found->second.request) ? found->second.result : rejected(Reason::kReplay);
    const bool acquisition = r.operation == Operation::kAcquire;
    const bool safety = Safety(r.operation);
    const bool duration = acquisition || r.operation == Operation::kRenew;
    if ((acquisition || safety) && (r.lease.generation || !r.lease.token.empty()))
        return rejected(Reason::kInvalidArgument);
    if ((!duration && r.requested_lease_duration_ns) || (duration &&
        (!r.requested_lease_duration_ns || r.requested_lease_duration_ns > capabilities_.max_lease_duration_ns ||
         AddOverflows(now, r.requested_lease_duration_ns)))) return rejected(Reason::kInvalidArgument);
    const Key stream{r.context.producer_session, acquisition ? "acquire" : "safety"};
    const auto cursor = replay_.find(stream);
    if ((acquisition || safety) && cursor != replay_.end() && r.context.sequence <= cursor->second)
        return rejected(Reason::kReplay);
    if (!acquisition && !safety) {
        if (!Owns(r.context, r.lease)) return rejected(Reason::kNotOwner);
        if (r.context.sequence <= management_sequence_) return rejected(Reason::kReplay);
    }
    // No live request or replay cursor is evicted. Stop paths still disable on exhaustion.
    if (cache_.size() >= kMaxRequests || ((acquisition || safety) && cursor == replay_.end() &&
        replay_.size() >= kMaxReplayStreams)) {
        Stop();
        status_.fault_reason = Reason::kBusy;
        return {ResultCode::kFailed, Reason::kBusy, status_.state, {}, 0};
    }
    ControlResult result{ResultCode::kCompleted, Reason::kNone, status_.state, {}, 0};
    const auto fail = [&](Reason reason, bool executed = false) {
        result.code = executed ? ResultCode::kFailed : ResultCode::kRejected;
        result.reason = reason;
    };
    switch (r.operation) {
        case Operation::kAcquire: {
            if (lease_.generation) { fail(Reason::kBusy); break; }
            if (status_.state != State::kDisarmed) { fail(Reason::kInvalidState); break; }
            if (generation_ == std::numeric_limits<std::uint64_t>::max()) {
                Stop(); fail(Reason::kInvalidState, true); break;
            }
            std::string token;
            try { token = tokens_(); } catch (...) { }
            if (!Id(token)) { Fault(); fail(Reason::kBackendFailure, true); break; }
            Revoke();
            lease_ = {++generation_, std::move(token)};
            status_.owner_generation = lease_.generation;
            status_.owner_producer_session = r.context.producer_session;
            status_.lease_expires_time_ns = now + r.requested_lease_duration_ns;
            result.lease = lease_; result.lease_expires_time_ns = status_.lease_expires_time_ns;
            break;
        }
        case Operation::kRenew:
            status_.lease_expires_time_ns = now + r.requested_lease_duration_ns;
            result.lease = lease_; result.lease_expires_time_ns = status_.lease_expires_time_ns;
            break;
        case Operation::kArm:
            if (status_.state != State::kDisarmed || status_.paused || !backend_.Ready()) {
                fail(Reason::kInvalidState); break;
            }
            status_.state = State::kArmed; arm_time_ns_ = now;
            break;
        case Operation::kRelease:
        case Operation::kDisarm: {
            const auto state = status_.state;
            Revoke();
            if (!Disable()) { Fault(); fail(Reason::kBackendFailure, true); }
            else if (state != State::kFault && state != State::kEmergencyStop)
                status_.state = State::kDisarmed;
            break;
        }
        case Operation::kEmergencyStop:
            Revoke();
            status_.state = State::kEmergencyStop;
            fault_time_ns_ = now;
            if (!backend_.EmergencyStop()) {
                status_.backend_failure = true; Disable(); fail(Reason::kBackendFailure, true);
            }
            break;
        case Operation::kClearFault:
            if (status_.state != State::kFault && status_.state != State::kEmergencyStop) {
                fail(Reason::kInvalidState); break;
            }
            if (r.context.issued_time_ns <= fault_time_ns_) { fail(Reason::kReplay); break; }
            if (!Disable() || !backend_.ClearFault()) {
                status_.backend_failure = true; Fault(); fail(Reason::kBackendFailure, true);
            } else {
                Revoke(); status_.state = State::kDisarmed;
                status_.backend_failure = false; status_.fault_reason = Reason::kNone;
                fault_time_ns_ = 0;
            }
            break;
        default: fail(Reason::kInvalidArgument); break;
    }
    if (result.code != ResultCode::kRejected) {
        if (acquisition || safety) replay_[stream] = r.context.sequence;
        else if (lease_.generation) management_sequence_ = r.context.sequence;
    }
    result.state = status_.state;
    cache_.emplace(key, Cached{r, result});
    return result;
}

Decision Guard::SetPaused(bool paused, std::uint64_t now) {
    Tick(now);
    if (!capabilities_.simulation || status_.state == State::kStopped) return {Reason::kInvalidState};
    if (paused && !status_.paused) {
        status_.paused = true;
        Revoke();
        if (!Disable()) { Fault(); return {Reason::kBackendFailure}; }
        if (status_.state == State::kArmed) status_.state = State::kDisarmed;
    }
    status_.paused = paused;
    return {};
}

bool Guard::Reset(std::string session) {
    if (!Id(session) || session == capabilities_.controller_session) {
        Fault(Reason::kInvalidArgument); return false;
    }
    Revoke();
    cache_.clear(); replay_.clear(); generation_ = last_now_ns_ = fault_time_ns_ = 0;
    capabilities_.controller_session = std::move(session);
    status_ = {};
    if (!Disable()) {
        status_.state = State::kFault; status_.fault_reason = Reason::kBackendFailure; return false;
    }
    return true;
}

}  // namespace openember::actuator::control
