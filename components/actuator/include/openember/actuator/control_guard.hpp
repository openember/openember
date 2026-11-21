#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "openember/actuator/joint_types.hpp"

namespace openember::actuator::control {

enum class State { kDisarmed, kArmed, kFault, kEmergencyStop, kStopped };
enum class Operation { kUnspecified, kAcquire, kRenew, kRelease, kArm,
                       kDisarm, kEmergencyStop, kClearFault };
enum class Reason { kNone, kInvalidArgument, kWrongContext, kClockMismatch,
                    kExpired, kReplay, kNotOwner, kBusy, kInvalidState,
                    kTopologyMismatch, kUnsupportedMode, kLimitViolation,
                    kBackendFailure };
enum class ResultCode { kCompleted, kRejected, kFailed };

struct Decision {
    Reason reason = Reason::kNone;
    bool Ok() const noexcept { return reason == Reason::kNone; }
};

struct JointCapability {
    std::string joint_name;
    bool continuous = false;
    double min_position_rad = 0, max_position_rad = 0;
    double max_velocity_radps = 0, max_effort_nm = 0;
    double max_stiffness_nm_per_rad = 0, max_damping_nms_per_rad = 0;
    std::vector<JointControlMode> supported_modes;
    std::uint64_t max_feedback_age_ns = 0;
};

struct Capabilities {
    std::uint32_t contract_version = 0;
    std::string robot_id, controller_id, controller_session, model_revision, clock_domain;
    bool simulation = false;
    std::uint64_t max_command_duration_ns = 0, max_lease_duration_ns = 0, command_timeout_ns = 0;
    std::vector<JointCapability> joints;
};

struct Context {
    std::string robot_id, controller_id, controller_session, model_revision, producer_session;
    std::uint64_t sequence = 0;
    std::string clock_domain;
    std::uint64_t issued_time_ns = 0, expires_time_ns = 0;
};

struct Lease {
    std::uint64_t generation = 0;
    std::string token;
};

struct Target {
    std::string joint_name;
    JointControlMode mode = JointControlMode::kUnspecified;
    double position_rad = 0, velocity_radps = 0, effort_nm = 0;
    double stiffness_nm_per_rad = 0, damping_nms_per_rad = 0, max_effort_nm = 0;
};

struct Command { Context context; Lease lease; std::vector<Target> joints; };
struct Request {
    Context context;
    std::string request_id;
    Operation operation = Operation::kUnspecified;
    Lease lease;
    std::uint64_t requested_lease_duration_ns = 0;
};

// Supplied by the authenticated endpoint, never derived from message fields.
struct Permission { bool control = false, stop = false, clear_fault = false; };
struct ControlResult {
    ResultCode code = ResultCode::kRejected;
    Reason reason = Reason::kInvalidArgument;
    State state = State::kDisarmed;
    Lease lease;
    std::uint64_t lease_expires_time_ns = 0;
};

// Public diagnostics deliberately exclude the lease token.
struct Status {
    State state = State::kDisarmed;
    bool paused = false;
    bool backend_failure = false;
    Reason fault_reason = Reason::kNone;
    std::uint64_t owner_generation = 0;
    std::string owner_producer_session;
    std::uint64_t lease_expires_time_ns = 0;
    std::uint64_t last_accepted_command_sequence = 0, last_applied_command_sequence = 0;
    std::uint64_t last_accepted_command_time_ns = 0, rejected_command_count = 0;
    Reason last_rejection_reason = Reason::kNone;
};

Decision ValidateCapabilities(const Capabilities& capabilities);
// Requires capabilities that passed ValidateCapabilities; Guard validates once
// at construction and keeps them immutable for the session.
Decision ValidateTargets(const Capabilities& capabilities, const std::vector<Target>& targets);
bool SampleFresh(std::uint64_t sequence, std::uint64_t receive_time_ns,
                 bool receive_fresh, std::uint64_t max_age_ns, std::uint64_t now_ns) noexcept;

// All methods must be bounded, synchronous and non-reentrant. False means the
// operation did not complete. Disable must be idempotent and use the backend's
// actual disable path. Apply must enforce final computed torque/actuator limits,
// reject non-finite results, and must not retain the target references.
// Independent device/watchdog protection is required if this process stalls.
class Backend {
public:
    virtual ~Backend() = default;
    virtual bool Ready() noexcept = 0;
    virtual bool Apply(const std::vector<Target>& targets) noexcept = 0;
    virtual bool Disable() noexcept = 0;
    virtual bool EmergencyStop() noexcept = 0;
    virtual bool ClearFault() noexcept = 0;
};

// Single owner: call Tick on every I/O cycle, even without incoming commands.
// now_ns must be CLOCK_MONOTONIC in capabilities.clock_domain. TokenFactory must
// return a fresh cryptographically random ASCII token (1..63 bytes). Backend
// outlives Guard, including its destructor. No transport or threads are created.
class Guard {
public:
    using TokenFactory = std::function<std::string()>;
    Guard(Capabilities capabilities, Backend& backend, TokenFactory tokens);
    ~Guard();
    Guard(const Guard&) = delete;
    Guard& operator=(const Guard&) = delete;

    const Capabilities& Info() const noexcept { return capabilities_; }
    Status Snapshot() const { return status_; }
    ControlResult Control(const Request& request, std::uint64_t now_ns, Permission permission);
    Decision Accept(const Command& command, std::uint64_t now_ns);
    // Trusted owner preflight for asynchronous physical enable. Does not ARM,
    // consume a request/sequence or start the command watchdog. Tick may revoke
    // expired authority. Caller must still commit Control(ARM) after evidence.
    Decision CheckArm(const Request&, std::uint64_t now_ns, Permission);
    // Current lease only: does not reuse an old request's execution deadline.
    // Read-only and safe inside a device write gate; does not call the backend.
    Decision CheckLease(const std::string& producer, const Lease&, std::uint64_t now_ns) const;
    // Adapter rejection: account for a malformed command without changing the
    // accepted cursor/target or refreshing its watchdog; Tick still runs.
    Decision RejectInput(Reason reason, std::uint64_t now_ns);
    Decision Apply(std::uint64_t now_ns);
    void Tick(std::uint64_t now_ns);
    // Owner-side feedback/interlock failure, never a network management action.
    void TripFault(Reason reason, std::uint64_t now_ns);
    Decision SetPaused(bool paused, std::uint64_t now_ns);
    // Owner must supply a never-reused session ID after resetting the backend.
    // An invalid reset faults and clears targets, rather than resuming old control.
    bool Reset(std::string new_session);
    void Stop() noexcept;

private:
    using Key = std::pair<std::string, std::string>;
    struct Cached { Request request; ControlResult result; };
    Decision CheckContext(const Context& context, std::uint64_t now_ns) const;
    bool Owns(const Context& context, const Lease& lease) const;
    void Revoke() noexcept;
    void Fault(Reason reason = Reason::kBackendFailure) noexcept;
    bool Disable() noexcept;
    Decision Reject(Reason reason);

    Capabilities capabilities_;
    Backend& backend_;
    TokenFactory tokens_;
    Status status_;
    Lease lease_;
    std::uint64_t generation_ = 0, management_sequence_ = 0;
    std::uint64_t arm_time_ns_ = 0, last_now_ns_ = 0;
    std::uint64_t fault_time_ns_ = 0;
    std::optional<Command> target_;
    // Never evict live idempotency entries or replay cursors within a session.
    // Exhaustion fails closed; stopped sessions require a fresh session ID.
    std::map<Key, Cached> cache_;
    std::map<Key, std::uint64_t> replay_;
};

}  // namespace openember::actuator::control
