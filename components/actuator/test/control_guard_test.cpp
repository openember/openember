#include "openember/actuator/control_guard.hpp"

#include <algorithm>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace c = openember::actuator::control;
using Mode = openember::actuator::JointControlMode;
void Check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
void Is(c::Decision result, c::Reason expected) { Check(result.reason == expected, "unexpected decision"); }
constexpr c::Permission kAll{true, true, true};

c::Capabilities Capabilities() {
    c::Capabilities result;
    result.contract_version = 2;
    result.robot_id = "simulation"; result.controller_id = "joints";
    result.controller_session = "epoch-1"; result.model_revision = "model-1";
    result.clock_domain = "host-monotonic:boot-fixture"; result.simulation = true;
    result.max_command_duration_ns = 1000; result.max_lease_duration_ns = 10000;
    result.command_timeout_ns = 100;
    for (const auto* name : {"hip", "wheel"}) {
        c::JointCapability joint;
        joint.joint_name = name; joint.continuous = joint.joint_name == "wheel";
        joint.min_position_rad = -1; joint.max_position_rad = 1;
        joint.max_velocity_radps = 10; joint.max_effort_nm = 20;
        joint.max_stiffness_nm_per_rad = 100; joint.max_damping_nms_per_rad = 10;
        joint.max_feedback_age_ns = 20;
        joint.supported_modes = {Mode::kDisabled, Mode::kPosition, Mode::kVelocity, Mode::kEffort, Mode::kMit};
        result.joints.push_back(joint);
    }
    return result;
}

struct FakeBackend : c::Backend {
    bool ready = true, disable_ok = true, apply_ok = true, clear_ok = true, estop_ok = true;
    bool enabled = false;
    int writes = 0, disables = 0, clears = 0, estops = 0;
    std::vector<c::Target> last;
    bool Ready() noexcept override { return ready; }
    bool Apply(const std::vector<c::Target>& targets) noexcept override {
        ++writes; last = targets;
        if (apply_ok) enabled = true;
        return apply_ok;
    }
    bool Disable() noexcept override { ++disables; if (disable_ok) enabled = false; return disable_ok; }
    bool EmergencyStop() noexcept override { ++estops; if (estop_ok) enabled = false; return estop_ok; }
    bool ClearFault() noexcept override { ++clears; return clear_ok; }
};

struct Fixture {
    FakeBackend backend;
    int tokens = 0;
    c::Guard guard;
    c::Lease lease;
    Fixture() : guard(Capabilities(), backend, [&] { return "token-" + std::to_string(++tokens); }) {}
    c::Context Context(std::uint64_t now = 1000, std::uint64_t sequence = 1,
                       std::string producer = "motion") const {
        const auto& info = guard.Info();
        return {info.robot_id, info.controller_id, info.controller_session, info.model_revision,
                std::move(producer), sequence, info.clock_domain, now, now + 500};
    }
    c::Request Request(c::Operation op, std::uint64_t now = 1000, std::uint64_t sequence = 1,
                       std::string id = "request") const {
        c::Request r;
        r.context = Context(now, sequence); r.request_id = std::move(id); r.operation = op;
        if (op == c::Operation::kAcquire || op == c::Operation::kRenew) r.requested_lease_duration_ns = 5000;
        if (op == c::Operation::kRenew || op == c::Operation::kRelease || op == c::Operation::kArm) r.lease = lease;
        return r;
    }
    void Acquire(std::uint64_t duration = 5000) {
        auto r = Request(c::Operation::kAcquire);
        r.requested_lease_duration_ns = duration;
        const auto result = guard.Control(r, 1000, kAll);
        Check(result.code == c::ResultCode::kCompleted, "acquire failed"); lease = result.lease;
    }
    void Arm() {
        Acquire();
        Check(guard.Control(Request(c::Operation::kArm, 1001, 1, "arm"), 1001, kAll).code ==
              c::ResultCode::kCompleted, "arm failed");
    }
    c::Command Command(std::uint64_t now = 1002, std::uint64_t sequence = 1) const {
        c::Command command{Context(now, sequence), lease, {}};
        for (const auto& joint : guard.Info().joints) {
            c::Target target;
            target.joint_name = joint.joint_name; target.mode = Mode::kMit;
            target.position_rad = .5; target.max_effort_nm = 10;
            target.stiffness_nm_per_rad = 20; target.damping_nms_per_rad = 1;
            command.joints.push_back(target);
        }
        return command;
    }
};

void TestCapabilities() {
    auto cap = Capabilities();
    Check(c::ValidateCapabilities(cap).Ok(), "valid capabilities rejected");
    const auto invalid = [](c::Capabilities value) { Check(!c::ValidateCapabilities(value).Ok(), "invalid capabilities accepted"); };
    auto bad = cap; bad.contract_version = 0; invalid(bad);
    bad = cap; bad.clock_domain = "host-monotonic:"; invalid(bad);
    bad = cap; bad.clock_domain = "unix"; invalid(bad);
    bad = cap; bad.joints.push_back(bad.joints[0]); invalid(bad);
    bad = cap; bad.joints[0].max_effort_nm = std::numeric_limits<double>::infinity(); invalid(bad);
    bad = cap; bad.joints[0].max_feedback_age_ns = 0; invalid(bad);
    bad = cap; bad.joints[0].supported_modes = {Mode::kMit, Mode::kMit}; invalid(bad);
    bad = cap; bad.joints[0].supported_modes = {static_cast<Mode>(99)}; invalid(bad);
    bad = cap; bad.robot_id = std::string(64, 'a'); invalid(bad);
    bad = cap; bad.controller_id = std::string("id\0tail", 7); invalid(bad);
    bad = cap; bad.command_timeout_ns = 0; invalid(bad);
    bad = cap;
    while (bad.joints.size() < 32) {
        auto joint = bad.joints[0]; joint.joint_name = "joint_" + std::to_string(bad.joints.size());
        bad.joints.push_back(joint);
    }
    Check(c::ValidateCapabilities(bad).Ok(), "32-joint capability rejected");
    auto extra = bad.joints[0]; extra.joint_name = "joint_32"; bad.joints.push_back(extra); invalid(bad);
    FakeBackend backend;
    bool threw = false;
    try { c::Guard guard(bad, backend, [] { return "token"; }); }
    catch (const std::invalid_argument&) { threw = true; }
    Check(threw, "bad capabilities did not fail construction");
    backend.disable_ok = false;
    c::Guard guard(cap, backend, [] { return "token"; });
    Check(guard.Snapshot().state == c::State::kFault && guard.Snapshot().backend_failure,
          "startup disable failure not latched");
}

void TestTargets() {
    Fixture f;
    auto targets = f.Command().joints;
    const auto validate = [&](const std::vector<c::Target>& values) { return c::ValidateTargets(f.guard.Info(), values); };
    Check(validate(targets).Ok(), "valid target rejected");
    std::reverse(targets.begin(), targets.end()); Check(validate(targets).Ok(), "order became topology");
    targets = f.Command().joints;
    auto bad = targets; bad.pop_back(); Is(validate(bad), c::Reason::kTopologyMismatch);
    bad = targets; bad[1] = bad[0]; Is(validate(bad), c::Reason::kTopologyMismatch);
    bad = targets; bad[1].joint_name = "alien"; Is(validate(bad), c::Reason::kTopologyMismatch);
    bad = targets; bad[0].mode = static_cast<Mode>(99); Is(validate(bad), c::Reason::kUnsupportedMode);
    bad = targets; bad[0].position_rad = std::numeric_limits<double>::quiet_NaN(); Is(validate(bad), c::Reason::kInvalidArgument);
    bad = targets; bad[0].velocity_radps = std::numeric_limits<double>::infinity(); Is(validate(bad), c::Reason::kInvalidArgument);
    bad = targets; bad[0].position_rad = 2; Is(validate(bad), c::Reason::kLimitViolation);
    bad = targets; bad[1].position_rad = 100; Check(validate(bad).Ok(), "continuous wheel used leg limit");
    bad = targets; bad[1].velocity_radps = 11; Is(validate(bad), c::Reason::kLimitViolation);
    bad = targets; bad[0].effort_nm = 11; Is(validate(bad), c::Reason::kLimitViolation);
    bad = targets; bad[0].stiffness_nm_per_rad = -1; Is(validate(bad), c::Reason::kLimitViolation);
    bad = targets; bad[0].max_effort_nm = 21; Is(validate(bad), c::Reason::kLimitViolation);
    bad = targets; bad[0].max_effort_nm = 0; Is(validate(bad), c::Reason::kLimitViolation);
    bad = targets; bad[0].damping_nms_per_rad = 11; Is(validate(bad), c::Reason::kLimitViolation);
    for (auto mode : {Mode::kDisabled, Mode::kPosition, Mode::kVelocity, Mode::kEffort}) {
        bad = targets; bad[0] = {}; bad[0].joint_name = "hip"; bad[0].mode = mode;
        if (mode != Mode::kDisabled) bad[0].max_effort_nm = 10;
        Check(validate(bad).Ok(), "simple mode rejected");
        bad[0].stiffness_nm_per_rad = 1; Is(validate(bad), c::Reason::kInvalidArgument);
    }
    auto cap = f.guard.Info(); cap.joints[0].supported_modes = {Mode::kDisabled};
    Is(c::ValidateTargets(cap, targets), c::Reason::kUnsupportedMode);
    f.Arm();
    auto partial = f.Command(); partial.joints[1].effort_nm = 11;
    Is(f.guard.Accept(partial, 1002), c::Reason::kLimitViolation);
    Check(f.backend.writes == 0 && f.guard.Snapshot().last_accepted_command_sequence == 0,
          "bad joint caused partial target acceptance");
}

void TestContext() {
    Fixture f; f.Arm(); const auto good = f.Command();
    const auto test = [&](const c::Command& command, c::Reason reason) { Is(f.guard.Accept(command, 1002), reason); };
    auto bad = good; bad.context.robot_id = "real-robot"; test(bad, c::Reason::kWrongContext);
    bad = good; bad.context.controller_session = "epoch-old"; test(bad, c::Reason::kWrongContext);
    bad = good; bad.context.model_revision = "other-model"; test(bad, c::Reason::kWrongContext);
    bad = good; bad.context.clock_domain = "host-monotonic:other-boot"; test(bad, c::Reason::kClockMismatch);
    bad = good; bad.context.issued_time_ns = 0; test(bad, c::Reason::kExpired);
    bad = good; bad.context.issued_time_ns = 1003; test(bad, c::Reason::kExpired);
    bad = good; bad.context.expires_time_ns = 1002; test(bad, c::Reason::kExpired);
    bad = good; bad.context.expires_time_ns = std::numeric_limits<std::uint64_t>::max(); test(bad, c::Reason::kExpired);
    bad = good; bad.context.sequence = 0; test(bad, c::Reason::kInvalidArgument);
    bad = good; bad.context.producer_session = std::string("bad\0id", 6); test(bad, c::Reason::kInvalidArgument);
    Check(f.guard.Snapshot().last_accepted_command_sequence == 0, "invalid context advanced cursor");
    Check(f.guard.Accept(good, 1002).Ok(), "rejection consumed sequence");
    test(good, c::Reason::kReplay);
    Check(f.guard.Snapshot().last_accepted_command_time_ns == 1002, "duplicate refreshed watchdog");
}

void TestLease() {
    Fixture f; f.Acquire();
    Check(!f.backend.enabled && f.guard.Snapshot().state == c::State::kDisarmed, "acquire enabled output");
    auto other = f.Request(c::Operation::kAcquire, 1001, 1, "other");
    other.context.producer_session = "other";
    Check(f.guard.Control(other, 1001, kAll).reason == c::Reason::kBusy, "second owner acquired");
    auto renew = f.Request(c::Operation::kRenew, 1002, 1, "renew");
    renew.lease.token = "wrong";
    Check(f.guard.Control(renew, 1002, kAll).reason == c::Reason::kNotOwner, "wrong token renewed");
    auto release = f.Request(c::Operation::kRelease, 1003, 1, "release");
    Check(f.guard.Control(release, 1003, kAll).code == c::ResultCode::kCompleted, "release failed");
    const auto old = f.lease;
    auto acquire = f.Request(c::Operation::kAcquire, 1004, 2, "new-owner");
    f.lease = f.guard.Control(acquire, 1004, kAll).lease;
    Check(f.lease.generation > old.generation && f.lease.token != old.token, "lease generation reused");
    renew = f.Request(c::Operation::kRenew, 1005, 1, "stale-renew"); renew.lease = old;
    Check(f.guard.Control(renew, 1005, kAll).reason == c::Reason::kNotOwner, "old lease revived");
    f.guard.Tick(6004);
    Check(f.guard.Snapshot().state == c::State::kFault && !f.guard.Snapshot().owner_generation,
          "lease expiry not latched");
    renew = f.Request(c::Operation::kRenew, 6004, 2, "expired-renew");
    Check(f.guard.Control(renew, 6004, kAll).code == c::ResultCode::kRejected, "expired lease renewed");
}

void TestIdempotency() {
    Fixture f;
    auto acquire = f.Request(c::Operation::kAcquire);
    const auto first = f.guard.Control(acquire, 1000, kAll);
    const auto retry = f.guard.Control(acquire, 1001, kAll);
    Check(first.lease.token == retry.lease.token && f.tokens == 1 &&
          first.lease_expires_time_ns == retry.lease_expires_time_ns, "acquire retry executed again");
    f.lease = first.lease;
    auto renew = f.Request(c::Operation::kRenew, 1002, 1, "renew");
    const auto renewed = f.guard.Control(renew, 1002, kAll);
    Check(f.guard.Control(renew, 1003, kAll).lease_expires_time_ns == renewed.lease_expires_time_ns,
          "renew retry extended lease");
    auto conflict = renew; conflict.requested_lease_duration_ns += 1;
    Check(f.guard.Control(conflict, 1003, kAll).reason == c::Reason::kReplay, "conflicting request ID accepted");
    Check(f.guard.Control(renew, 1502, kAll).reason == c::Reason::kExpired, "expired cached request executed");
    Check(f.guard.Snapshot().lease_expires_time_ns == renewed.lease_expires_time_ns, "expired retry extended lease");
    auto replay = f.Request(c::Operation::kRenew, 1503, 1, "new-id-old-seq");
    Check(f.guard.Control(replay, 1503, kAll).reason == c::Reason::kReplay, "old management sequence accepted");
}

void TestWatchdog() {
    { Fixture f; f.Arm(); f.guard.Tick(1101);
      Check(f.guard.Snapshot().state == c::State::kFault && f.backend.writes == 0 &&
            f.guard.Snapshot().fault_reason == c::Reason::kExpired, "missing first target not faulted"); }
    { Fixture f; f.Arm(); Check(f.guard.Accept(f.Command(), 1002).Ok(), "target rejected");
      auto invalid = f.Command(1050, 2); invalid.joints[0].effort_nm = 11;
      Is(f.guard.Accept(invalid, 1050), c::Reason::kLimitViolation);
      auto renew = f.Request(c::Operation::kRenew, 1051, 2, "renew");
      Check(f.guard.Control(renew, 1051, kAll).code == c::ResultCode::kCompleted, "renew failed");
      f.guard.Tick(1102); Check(f.guard.Snapshot().state == c::State::kFault, "invalid target or renew refreshed watchdog"); }
    { Fixture f; f.Arm(); auto target = f.Command(); target.context.expires_time_ns = 1022;
      Check(f.guard.Accept(target, 1002).Ok(), "short target rejected"); f.guard.Tick(1022);
      Check(f.guard.Snapshot().state == c::State::kFault, "target expiration ignored"); }
    { Fixture f; f.Acquire(5);
      Check(f.guard.Control(f.Request(c::Operation::kArm, 1001, 1, "arm"), 1001, kAll).code == c::ResultCode::kCompleted, "arm failed");
      Check(f.guard.Accept(f.Command(), 1002).Ok(), "target rejected"); f.guard.Tick(1005);
      Check(f.guard.Snapshot().state == c::State::kFault, "short lease expiration ignored"); }
}

void TestExecution() {
    Fixture f; f.Arm();
    Check(f.guard.Accept(f.Command(), 1002).Ok(), "first target rejected");
    auto latest = f.Command(1003, 2); latest.joints[0].position_rad = .75;
    Check(f.guard.Accept(latest, 1003).Ok(), "latest target rejected");
    Check(f.backend.writes == 0 && f.guard.Snapshot().last_applied_command_sequence == 0,
          "acceptance confused with application");
    Check(f.guard.Apply(1004).Ok() && f.backend.last[0].position_rad == .75 &&
          f.guard.Snapshot().last_applied_command_sequence == 2, "old queued target applied");
    auto expiring = f.Command(1005, 3); expiring.context.expires_time_ns = 1006;
    Check(f.guard.Accept(expiring, 1005).Ok(), "expiring target rejected");
    Check(!f.guard.Apply(1006).Ok() && f.backend.writes == 1 && !f.backend.enabled,
          "target applied after expiry");
    Fixture failed; failed.Arm(); failed.backend.apply_ok = false;
    Check(failed.guard.Accept(failed.Command(), 1002).Ok(), "target rejected");
    Is(failed.guard.Apply(1003), c::Reason::kBackendFailure);
    Check(failed.guard.Snapshot().state == c::State::kFault && failed.guard.Snapshot().backend_failure &&
          failed.guard.Snapshot().last_applied_command_sequence == 0, "write failure not latched");
    Fixture not_ready; not_ready.Acquire(); not_ready.backend.ready = false;
    Check(not_ready.guard.Control(not_ready.Request(c::Operation::kArm, 1001, 1, "arm"), 1001, kAll).code ==
          c::ResultCode::kRejected && !not_ready.backend.enabled, "unready backend armed");
    Fixture disconnected; disconnected.Arm();
    Check(disconnected.guard.Accept(disconnected.Command(), 1002).Ok(), "target rejected");
    disconnected.backend.ready = false;
    Is(disconnected.guard.Apply(1003), c::Reason::kBackendFailure);
    Check(disconnected.backend.writes == 0 && disconnected.guard.Snapshot().state == c::State::kFault,
          "backend readiness loss still wrote targets");
}

void TestSafety() {
    Fixture f;
    Check(f.guard.Control(f.Request(c::Operation::kAcquire), 1000, {}).code == c::ResultCode::kRejected,
          "unauthorized acquire accepted");
    f.Arm(); Check(f.guard.Accept(f.Command(), 1002).Ok() && f.guard.Apply(1002).Ok(), "target failed");
    auto estop = f.Request(c::Operation::kEmergencyStop, 1003, 999, "estop"); estop.context.producer_session = "operator";
    Check(f.guard.Control(estop, 1003, kAll).code == c::ResultCode::kCompleted && !f.backend.enabled &&
          f.guard.Snapshot().state == c::State::kEmergencyStop, "emergency stop failed");
    auto early_clear = f.Request(c::Operation::kClearFault, 1002, 1, "clear-before-emergency");
    const auto early = f.guard.Control(early_clear, 1003, kAll);
    Check(early.code == c::ResultCode::kRejected && early.reason == c::Reason::kReplay &&
          f.guard.Snapshot().state == c::State::kEmergencyStop, "queued old clear unlocked a newer emergency");
    auto disarm = f.Request(c::Operation::kDisarm, 1004, 1, "disarm");
    Check(f.guard.Control(disarm, 1004, kAll).state == c::State::kEmergencyStop, "disarm cleared latch");
    auto clear = f.Request(c::Operation::kClearFault, 1005, 2, "clear");
    Check(f.guard.Control(clear, 1005, {true, true, false}).code == c::ResultCode::kRejected,
          "fault clear bypassed authorization");
    f.backend.clear_ok = false;
    Check(f.guard.Control(clear, 1005, kAll).code == c::ResultCode::kFailed &&
          f.guard.Snapshot().state == c::State::kEmergencyStop, "backend clear failure unlocked");
    f.backend.clear_ok = true;
    clear = f.Request(c::Operation::kClearFault, 1006, 3, "clear-again");
    Check(f.guard.Control(clear, 1006, kAll).state == c::State::kDisarmed &&
          !f.guard.Snapshot().owner_generation && !f.backend.enabled, "clear reacquired or enabled");
    // The operator's high sequence must not poison the motion producer's stream.
    auto motion_stop = f.Request(c::Operation::kDisarm, 1007, 4, "motion-stop");
    Check(f.guard.Control(motion_stop, 1007, kAll).code == c::ResultCode::kCompleted,
          "other producer consumed safety cursor");
    f.guard.Stop();
    auto request = f.Request(c::Operation::kAcquire, 1008, 2, "stopped");
    Check(f.guard.Control(request, 1008, kAll).code == c::ResultCode::kRejected, "stopped session revived");
    Fixture disable_failed; disable_failed.Arm();
    Check(disable_failed.guard.Accept(disable_failed.Command(), 1002).Ok() &&
          disable_failed.guard.Apply(1002).Ok(), "target failed");
    disable_failed.backend.disable_ok = false;
    const auto failure = disable_failed.guard.Control(
        disable_failed.Request(c::Operation::kDisarm, 1003, 1, "failed-disarm"), 1003, kAll);
    Check(failure.code == c::ResultCode::kFailed && failure.state == c::State::kFault &&
          disable_failed.guard.Snapshot().backend_failure, "disable failure announced completion");
    Fixture estop_failed; estop_failed.backend.estop_ok = false;
    Check(estop_failed.guard.Control(estop_failed.Request(c::Operation::kEmergencyStop), 1000, kAll).code ==
          c::ResultCode::kFailed && estop_failed.guard.Snapshot().state == c::State::kEmergencyStop &&
          estop_failed.guard.Snapshot().backend_failure, "failed emergency stop lost latch");
}

void TestPauseReset() {
    Fixture f; f.Arm(); const auto old = f.Command();
    Check(f.guard.Accept(old, 1002).Ok() && f.guard.Apply(1002).Ok(), "target failed");
    Check(f.guard.SetPaused(true, 1003).Ok() && !f.backend.enabled &&
          f.guard.Snapshot().state == c::State::kDisarmed && !f.guard.Snapshot().owner_generation,
          "pause retained authority");
    Is(f.guard.Accept(old, 1004), c::Reason::kInvalidState);
    auto acquire = f.Request(c::Operation::kAcquire, 1004, 2, "paused-acquire");
    f.lease = f.guard.Control(acquire, 1004, kAll).lease;
    Check(f.guard.Control(f.Request(c::Operation::kArm, 1005, 1, "paused-arm"), 1005, kAll).code ==
          c::ResultCode::kRejected, "paused arm accepted");
    Check(f.guard.SetPaused(false, 1006).Ok() && f.guard.Snapshot().state == c::State::kDisarmed,
          "resume automatically armed");
    Check(!f.guard.Reset("epoch-1"), "reset reused session");
    Check(f.guard.Reset("epoch-2") && !f.guard.Snapshot().owner_generation &&
          f.guard.Snapshot().last_accepted_command_sequence == 0, "reset retained old targets");
    Is(f.guard.Accept(old, 1007), c::Reason::kWrongContext);
    f.guard.Stop(); Check(f.guard.Reset("epoch-3"), "new session could not restart stopped guard");
    Fixture invalid_reset; invalid_reset.Arm();
    Check(invalid_reset.guard.Accept(invalid_reset.Command(), 1002).Ok() && invalid_reset.guard.Apply(1002).Ok(),
          "target failed");
    Check(!invalid_reset.guard.Reset("epoch-1") && !invalid_reset.backend.enabled &&
          invalid_reset.guard.Snapshot().state == c::State::kFault &&
          !invalid_reset.guard.Snapshot().owner_generation, "invalid reset retained old control");
    Fixture pause_failed; pause_failed.backend.disable_ok = false;
    Is(pause_failed.guard.SetPaused(true, 1000), c::Reason::kBackendFailure);
    Check(pause_failed.guard.Snapshot().paused && pause_failed.guard.Snapshot().state == c::State::kFault,
          "failed pause lost pause state");
    auto cap = Capabilities(); cap.simulation = false;
    FakeBackend backend; c::Guard real(cap, backend, [] { return "token"; });
    Is(real.SetPaused(true, 1000), c::Reason::kInvalidState);
}

void TestResources() {
    Fixture f; f.Acquire();
    for (int i = 1; i <= 127; ++i) {
        auto renew = f.Request(c::Operation::kRenew, 1000, i, "renew-" + std::to_string(i));
        Check(f.guard.Control(renew, 1000, kAll).code == c::ResultCode::kCompleted, "cache filled too early");
    }
    auto stop = f.Request(c::Operation::kEmergencyStop, 1000, 1, "stop-on-full-cache");
    const auto result = f.guard.Control(stop, 1000, kAll);
    Check(result.code == c::ResultCode::kFailed && result.reason == c::Reason::kBusy &&
          result.state == c::State::kStopped && !f.backend.enabled, "cache exhaustion bypassed stop");
    Fixture streams;
    for (int i = 0; i < 128; ++i) {
        const auto now = 1000 + i * 2;
        auto disarm = streams.Request(c::Operation::kDisarm, now, 1, "stop");
        disarm.context.producer_session = "producer-" + std::to_string(i);
        disarm.context.expires_time_ns = now + 1;
        Check(streams.guard.Control(disarm, now, kAll).code == c::ResultCode::kCompleted,
              "replay cursors filled too early");
    }
    auto exhausted = streams.Request(c::Operation::kDisarm, 1300, 1, "stop");
    exhausted.context.producer_session = "new-producer";
    Check(streams.guard.Control(exhausted, 1300, kAll).state == c::State::kStopped,
          "replay cursor exhaustion evicted live cursor");
    // Overflow is checked before adding duration to a host timestamp.
    Fixture overflow;
    const auto now = std::numeric_limits<std::uint64_t>::max() - 10;
    auto request = overflow.Request(c::Operation::kAcquire);
    request.context.issued_time_ns = now; request.context.expires_time_ns = now + 5;
    Check(overflow.guard.Control(request, now, kAll).reason == c::Reason::kInvalidArgument,
          "lease deadline addition overflowed");
    Fixture bad_token;
    FakeBackend backend;
    c::Guard guard(Capabilities(), backend, []() -> std::string { throw std::runtime_error("entropy"); });
    Check(guard.Control(bad_token.Request(c::Operation::kAcquire), 1000, kAll).code == c::ResultCode::kFailed &&
          guard.Snapshot().state == c::State::kFault, "token generation failure not closed");
}

void TestFeedback() {
    Check(c::SampleFresh(1, 100, true, 20, 120), "fresh boundary rejected");
    Check(!c::SampleFresh(1, 100, true, 20, 121), "stale joint became fresh by group publication");
    Check(!c::SampleFresh(0, 100, true, 20, 100), "no sample treated as measured");
    Check(!c::SampleFresh(1, 0, true, 20, 10), "missing receive time accepted");
    Check(!c::SampleFresh(1, 101, true, 20, 100), "future sample accepted");
    Check(!c::SampleFresh(1, 100, false, 20, 100), "invalid freshness accepted");
    Check(!c::SampleFresh(1, 100, true, 0, 100), "zero age limit accepted");
}

void TestClock() {
    Fixture f; f.Arm(); Check(f.guard.Accept(f.Command(), 1002).Ok(), "target failed");
    f.guard.Tick(1001);
    Check(f.guard.Snapshot().state == c::State::kFault &&
          f.guard.Snapshot().fault_reason == c::Reason::kClockMismatch, "backward clock not faulted");
    auto clear = f.Request(c::Operation::kClearFault, 1001, 1, "clear-backward");
    Check(f.guard.Control(clear, 1001, kAll).code == c::ResultCode::kRejected, "backward time cleared fault");
    Fixture zero; zero.guard.Tick(0);
    Check(zero.guard.Snapshot().state == c::State::kFault, "zero clock accepted");
}

void TestArmPreflight() {
    Fixture f; f.Acquire(); auto arm = f.Request(c::Operation::kArm, 1001, 1, "async-arm");
    const auto writes = f.backend.writes, disables = f.backend.disables;
    Is(f.guard.CheckArm(arm, 1001, {}), c::Reason::kNotOwner);
    Check(f.guard.CheckArm(arm, 1001, kAll).Ok(), "valid arm preflight failed");
    f.guard.Tick(1200); // Longer than command timeout, still not physically armed.
    Check(f.guard.Snapshot().state == c::State::kDisarmed && f.backend.writes == writes &&
          f.backend.disables == disables, "preflight started watchdog or wrote backend");
    Check(f.guard.CheckLease("motion", f.lease, 1200).Ok(), "valid lease rejected");
    Is(f.guard.CheckLease("other", f.lease, 1200), c::Reason::kNotOwner);
    Check(f.guard.Control(arm, 1200, kAll).code == c::ResultCode::kCompleted, "preflight consumed ARM request");
    Fixture bad; bad.Acquire(); auto r = bad.Request(c::Operation::kArm, 1001, 1, "bad");
    r.lease.token = "wrong"; Is(bad.guard.CheckArm(r, 1001, kAll), c::Reason::kNotOwner);
    r.lease = bad.lease; r.requested_lease_duration_ns = 1;
    Is(bad.guard.CheckArm(r, 1001, kAll), c::Reason::kInvalidArgument);
    Is(bad.guard.CheckLease("motion", bad.lease, 6000), c::Reason::kExpired);
    Fixture full; full.Acquire();
    for (std::uint64_t i = 1; i < 128; ++i)
        Check(full.guard.Control(full.Request(c::Operation::kRenew, 1001, i, "renew-" + std::to_string(i)), 1001, kAll).code ==
              c::ResultCode::kCompleted, "preflight resource fixture failed");
    Is(full.guard.CheckArm(full.Request(c::Operation::kArm, 1002, 128, "arm-full"), 1002, kAll), c::Reason::kBusy);
}

int main(int argc, char** argv) {
    try {
        Check(argc == 2, "test case required");
        const std::string name(argv[1]);
        if (name == "capabilities") TestCapabilities();
        else if (name == "targets") TestTargets();
        else if (name == "context") TestContext();
        else if (name == "lease") TestLease();
        else if (name == "idempotency") TestIdempotency();
        else if (name == "watchdog") TestWatchdog();
        else if (name == "execution") TestExecution();
        else if (name == "safety") TestSafety();
        else if (name == "pause_reset") TestPauseReset();
        else if (name == "resources") TestResources();
        else if (name == "feedback") TestFeedback();
        else if (name == "clock") TestClock();
        else if (name == "arm_preflight") TestArmPreflight();
        else throw std::runtime_error("unknown test case");
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
