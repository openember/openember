#include "openember/services/hardware_interface/joint_control_session.hpp"

#include <google/protobuf/message.h>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace hi = openember::services::hardware_interface;
namespace a = hi::actuator_control;
namespace d = a::domain;
namespace w = a::wire;
using Mode = openember::actuator::JointControlMode;
void Check(bool ok, const char* why) { if (!ok) throw std::runtime_error(why); }
template <typename T> T Wire(const T& original) {
    T result; Check(result.ParseFromString(original.SerializeAsString()), "wire round trip failed"); return result;
}
void Unknown(google::protobuf::Message* message, const char* field) {
    message->GetReflection()->SetEnumValue(message, message->GetDescriptor()->FindFieldByName(field), 999);
}
d::Capabilities Caps() {
    d::Capabilities c;
    c.contract_version = 2; c.robot_id = "sim-robot"; c.controller_id = "joints";
    c.controller_session = "epoch-1"; c.model_revision = "model-1"; c.clock_domain = "host-monotonic:boot-fixture";
    c.simulation = true; c.max_command_duration_ns = 1000; c.max_lease_duration_ns = 10000; c.command_timeout_ns = 100;
    for (const char* name : {"hip", "wheel"}) {
        d::JointCapability joint;
        joint.joint_name = name; joint.continuous = joint.joint_name == "wheel";
        joint.min_position_rad = -1; joint.max_position_rad = 1; joint.max_velocity_radps = 10; joint.max_effort_nm = 20;
        joint.max_stiffness_nm_per_rad = 100; joint.max_damping_nms_per_rad = 10; joint.max_feedback_age_ns = 20;
        joint.supported_modes = {Mode::kDisabled, Mode::kPosition, Mode::kVelocity, Mode::kEffort, Mode::kMit};
        c.joints.push_back(joint);
    }
    return c;
}
w::CommandContext Context(std::uint64_t now = 1000, std::uint64_t sequence = 1) {
    const auto c = Caps(); w::CommandContext context;
    context.set_robot_id(c.robot_id); context.set_controller_id(c.controller_id); context.set_controller_session(c.controller_session);
    context.set_model_revision(c.model_revision); context.set_producer_session("motion"); context.set_clock_domain(c.clock_domain);
    context.set_sequence(sequence); context.set_issued_time_ns(now); context.set_expires_time_ns(now + 500); return context;
}
w::ControlRequest Request(w::ControlOperation operation = w::CONTROL_OPERATION_ACQUIRE,
                         std::uint64_t now = 1000, std::uint64_t sequence = 1) {
    w::ControlRequest r; *r.mutable_context() = Context(now, sequence);
    r.set_request_id("request-" + std::to_string(operation) + "-" + std::to_string(sequence)); r.set_operation(operation);
    if (operation == w::CONTROL_OPERATION_ACQUIRE || operation == w::CONTROL_OPERATION_RENEW) r.set_requested_lease_duration_ns(5000);
    return r;
}
w::JointCommand Command(std::uint64_t now = 1002, std::uint64_t sequence = 1) {
    w::JointCommand message; *message.mutable_context() = Context(now, sequence);
    message.mutable_lease()->set_generation(7); message.mutable_lease()->set_token("private-token");
    for (const char* name : {"hip", "wheel"}) {
        auto* joint = message.add_joints(); joint->set_joint_name(name); joint->set_mode(w::JOINT_CONTROL_MODE_MIT);
        joint->set_position_rad(.5); joint->set_velocity_radps(-.25); joint->set_effort_nm(2);
        joint->set_stiffness_nm_per_rad(20); joint->set_damping_nms_per_rad(1); joint->set_max_effort_nm(10);
    }
    return message;
}
d::FeedbackFrame Frame(std::uint64_t now = 1004) {
    d::FeedbackFrame frame; frame.controller_session = Caps().controller_session; frame.clock_domain = Caps().clock_domain;
    frame.sequence = 1; frame.publish_time_ns = now; frame.physics_step = 10; frame.simulation_time_ns = 10000000;
    for (const char* name : {"hip", "wheel"}) {
        d::JointFeedback joint; joint.joint_name = name; joint.sample_sequence = 1; joint.receive_time_ns = 1003;
        joint.receive_fresh = true; joint.position_valid = joint.velocity_valid = joint.effort_valid = true;
        joint.position_rad = 0; joint.velocity_radps = -.25; joint.effort_nm = 1.5;
        frame.joints.push_back(joint);
    }
    return frame;
}
struct Backend : d::Backend {
    bool enabled = false, write_ok = true;
    int writes = 0, disables = 0;
    double last_position = 0;
    bool Ready() noexcept override { return true; }
    bool Apply(const std::vector<d::Target>& targets) noexcept override {
        ++writes; last_position = targets[0].position_rad; enabled = write_ok; return write_ok;
    }
    bool Disable() noexcept override { ++disables; enabled = false; return true; }
    bool EmergencyStop() noexcept override { enabled = false; return true; }
    bool ClearFault() noexcept override { return true; }
};
struct Session {
    Backend* backend;
    hi::JointControlSession session;
    w::ControlLease lease;
    Session() : Session(std::make_unique<Backend>()) {}
    explicit Session(std::unique_ptr<Backend> pointer)
        : backend(pointer.get()), session(Caps(), std::move(pointer), [] { return "random-token-fixture"; }) {}
    void Arm() {
        auto acquired = session.Control(Wire(Request()), 1000, {true, true, true});
        Check(acquired.Ok() && acquired.Value().code() == w::CONTROL_RESULT_CODE_COMPLETED, "acquire failed");
        lease = acquired.Value().lease();
        auto arm = Request(w::CONTROL_OPERATION_ARM, 1001); *arm.mutable_lease() = lease;
        auto result = session.Control(Wire(arm), 1001, {true, true, true});
        Check(result.Ok() && result.Value().code() == w::CONTROL_RESULT_CODE_COMPLETED && !backend->enabled, "arm failed or enabled early");
    }
    w::JointCommand Target(std::uint64_t now = 1002, std::uint64_t sequence = 1) const {
        auto target = Command(now, sequence); *target.mutable_lease() = lease; return target;
    }
};

void TestCommand() {
    const auto good = Command(); const auto converted = a::FromProto(Wire(good));
    Check(converted.Ok(), "valid command rejected"); const auto& c = converted.Value();
    Check(c.context.robot_id == "sim-robot" && c.context.controller_session == "epoch-1" && c.context.model_revision == "model-1" &&
          c.context.producer_session == "motion" && c.context.sequence == 1 && c.context.issued_time_ns == 1002 &&
          c.context.expires_time_ns == 1502 && c.lease.generation == 7 && c.lease.token == "private-token" &&
          c.joints.size() == 2 && c.joints[1].mode == Mode::kMit && c.joints[1].velocity_radps == -.25 &&
          c.joints[1].effort_nm == 2 && c.joints[1].stiffness_nm_per_rad == 20 &&
          c.joints[1].damping_nms_per_rad == 1 && c.joints[1].max_effort_nm == 10, "command fields changed");
    for (int i = 0; i < 10; ++i) {
        auto bad = good;
        switch (i) {
            case 0: bad.clear_context(); break;
            case 1: bad.clear_lease(); break;
            case 2: bad.mutable_context()->set_issued_time_ns(0); break;
            case 3: bad.mutable_context()->set_producer_session(std::string("id\0tail", 7)); break;
            case 4: bad.mutable_lease()->set_token(std::string(64, 'x')); break;
            case 5: bad.mutable_joints(1)->set_joint_name("hip"); break;
            case 6: Unknown(bad.mutable_joints(0), "mode"); break;
            case 7: bad.mutable_joints(0)->set_effort_nm(std::numeric_limits<double>::quiet_NaN()); break;
            case 8: bad.clear_joints(); break;
            case 9: while (bad.joints_size() < 33) { auto* j = bad.add_joints(); j->set_joint_name("j" + std::to_string(bad.joints_size())); } break;
        }
        Check(!a::FromProto(Wire(bad)).Ok(), "malformed command accepted");
    }
    auto boundary = good; boundary.mutable_context()->set_producer_session(std::string(63, 'p'));
    boundary.mutable_lease()->set_token(std::string(63, 't'));
    Check(a::FromProto(boundary).Ok(), "63-byte boundary rejected");
}

void TestRequest() {
    const auto good = Request();
    Check(a::FromProto(Wire(good)).Ok(), "valid acquire rejected");
    Check(!a::FromProto(w::ControlRequest{}).Ok(), "empty request accepted");
    auto bad = good; Unknown(&bad, "operation"); Check(!a::FromProto(Wire(bad)).Ok(), "unknown operation mapped");
    bad = good; bad.set_request_id(std::string(64, 'x')); Check(!a::FromProto(bad).Ok(), "oversized request ID accepted");
    bad = good; bad.mutable_lease()->set_token("unexpected"); Check(!a::FromProto(bad).Ok(), "acquire accepted stale lease");
    bad = Request(w::CONTROL_OPERATION_ARM); Check(!a::FromProto(bad).Ok(), "arm accepted missing lease");
    bad.mutable_lease()->set_generation(1); bad.mutable_lease()->set_token("token");
    Check(a::FromProto(bad).Ok(), "valid arm rejected");
    bad.set_requested_lease_duration_ns(1); Check(!a::FromProto(bad).Ok(), "arm accepted unrelated duration");
    bad = good; bad.set_requested_lease_duration_ns(0); Check(!a::FromProto(bad).Ok(), "zero lease duration accepted");
    bad = Request(w::CONTROL_OPERATION_DISARM); Check(a::FromProto(bad).Ok(), "independent disarm requires token");
}

void TestCapabilities() {
    const auto encoded = a::ToProto(Caps()); Check(encoded.Ok(), "capability encoding failed");
    const auto decoded = a::FromProto(Wire(encoded.Value()));
    Check(decoded.Ok() && decoded.Value().joints[1].continuous && decoded.Value().joints[0].supported_modes.size() == 5 &&
          decoded.Value().joints[0].max_feedback_age_ns == 20 && decoded.Value().clock_domain == Caps().clock_domain,
          "capability semantics lost");
    for (int i = 0; i < 7; ++i) {
        auto bad = encoded.Value();
        switch (i) {
            case 0: bad.set_contract_version(1); break;
            case 1: bad.set_clock_domain("unix"); break;
            case 2: bad.mutable_joints(0)->add_supported_modes(static_cast<w::JointControlMode>(999)); break;
            case 3: bad.mutable_joints(0)->set_max_effort_nm(std::numeric_limits<double>::infinity()); break;
            case 4: bad.mutable_joints(0)->set_max_feedback_age_ns(0); break;
            case 5: bad.mutable_joints(1)->set_joint_name("hip"); break;
            case 6: bad.set_model_revision(std::string(128, 'x')); break;
        }
        Check(!a::FromProto(Wire(bad)).Ok(), "invalid capability accepted");
    }
    auto bad = Caps(); bad.joints[0].supported_modes = {static_cast<Mode>(999)};
    Check(!a::ToProto(bad).Ok(), "unknown domain mode exported");
}

void TestResult() {
    const auto request = a::FromProto(Request()).Value();
    d::ControlResult result{d::ResultCode::kCompleted, d::Reason::kNone, d::State::kDisarmed, {1, "secret"}, 6000};
    auto encoded = a::ToProto(Caps(), request, result); Check(encoded.Ok(), "result encode failed");
    auto response = Wire(encoded.Value());
    Check(response.robot_id() == Caps().robot_id && response.controller_session() == "epoch-1" &&
          response.producer_session() == "motion" && response.request_sequence() == 1 && response.request_id() == request.request_id &&
          response.operation() == w::CONTROL_OPERATION_ACQUIRE && response.lease().token() == "secret" &&
          response.code() == w::CONTROL_RESULT_CODE_COMPLETED, "result association lost");
    result.code = d::ResultCode::kRejected; result.reason = d::Reason::kWrongContext;
    Check(!a::ToProto(Caps(), request, result).Ok(), "rejection disclosed token");
    result.lease = {}; result.lease_expires_time_ns = 0;
    auto stale = request; stale.context.controller_session = "old-epoch";
    auto rejected = a::ToProto(Caps(), stale, result);
    Check(rejected.Ok() && rejected.Value().controller_session() == "epoch-1" && !rejected.Value().has_lease(),
          "request impersonated response authority");
    result.state = static_cast<d::State>(999); Check(!a::ToProto(Caps(), request, result).Ok(), "unknown result state mapped");
}

void TestFeedback() {
    auto frame = Frame();
    frame.joints[0].temperature_celsius = std::numeric_limits<double>::quiet_NaN(); // unavailable channel
    frame.joints[0].voltage_v = 99; frame.joints[0].fault_code = 42;
    auto result = a::ToProto(Caps(), d::Status{}, frame); Check(result.Ok(), "feedback encode failed");
    const auto first = Wire(result.Value());
    Check(first.joints(0).position_valid() && first.joints(0).position_rad() == 0 &&
          first.joints(0).receive_time_ns() == 1003 && first.joints(0).sample_sequence() == 1 &&
          !first.joints(0).temperature_valid() && first.joints(0).temperature_celsius() == 0 &&
          !first.joints(0).voltage_valid() && first.joints(0).voltage_v() == 0 && first.joints(0).fault_code() == 0,
          "zero validity or unknown channels changed");
    frame.sequence = 2; frame.publish_time_ns = 1024;
    frame.joints[1].sample_sequence = 2; frame.joints[1].receive_time_ns = 1024;
    auto later = a::ToProto(Caps(), d::Status{}, frame);
    Check(later.Ok() && !later.Value().joints(0).receive_fresh() && later.Value().joints(1).receive_fresh() &&
          later.Value().joints(0).receive_time_ns() == 1003 && later.Value().joints(0).sample_sequence() == 1,
          "group publication refreshed stalled joint");
    for (int i = 0; i < 7; ++i) {
        auto bad = Frame();
        switch (i) {
            case 0: bad.controller_session = "old-epoch"; break;
            case 1: bad.clock_domain = "other-clock"; break;
            case 2: bad.joints[0].receive_time_ns = bad.publish_time_ns + 1; break;
            case 3: bad.joints[0].position_rad = std::numeric_limits<double>::infinity(); break;
            case 4: bad.joints[1] = bad.joints[0]; break;
            case 5: bad.message = std::string(160, 'x'); break;
            case 6: bad.physics_step = bad.simulation_time_ns = 0; break;
        }
        Check(!a::ToProto(Caps(), d::Status{}, bad).Ok(), "invalid frame exported");
    }
    auto initial = Frame(); initial.physics_step = initial.simulation_time_ns = 0;
    for (auto& j : initial.joints) { const auto name = j.joint_name; j = {}; j.joint_name = name; }
    Check(a::ToProto(Caps(), d::Status{}, initial).Ok(), "reset invalid samples rejected");
    d::Status fault; fault.state = d::State::kFault; fault.backend_failure = true; fault.fault_reason = d::Reason::kBackendFailure;
    auto failure = a::ToProto(Caps(), fault, Frame());
    Check(failure.Ok() && failure.Value().backend_failure() && failure.Value().fault_reason() == w::REJECTION_REASON_BACKEND_FAILURE,
          "backend fault diagnostics lost");
    auto cap = Caps(); cap.simulation = false;
    Check(!a::ToProto(cap, d::Status{}, Frame()).Ok(), "real feedback accepted simulation metadata");
}

void TestSession() {
    Session f; f.Arm();
    Check(f.session.Command(Wire(f.Target()), 1002).Ok() && f.backend->writes == 0, "accept applied target");
    auto latest = f.Target(1003, 2); latest.mutable_joints(0)->set_position_rad(.75);
    Check(f.session.Command(Wire(latest), 1003).Ok() && f.session.Apply(1004).Ok() && f.backend->last_position == .75,
          "wire/session/backend loop failed");
    auto state = f.session.Feedback(Frame());
    Check(state.Ok() && state.Value().last_accepted_command_sequence() == 2 && state.Value().last_applied_command_sequence() == 2 &&
          state.Value().owner_generation() == f.lease.generation() && state.Value().lease_expires_time_ns() == 6000 &&
          state.Value().SerializeAsString().find("random-token-fixture") == std::string::npos, "feedback leaked authority or lost cursors");
    auto wrong = f.Target(1005, 3); wrong.mutable_context()->set_controller_session("old-epoch");
    Check(f.session.Command(wrong, 1005).reason == d::Reason::kWrongContext, "old session accepted");
    auto estop = Request(w::CONTROL_OPERATION_EMERGENCY_STOP, 1006); estop.mutable_context()->set_producer_session("operator");
    auto result = f.session.Control(Wire(estop), 1006, {false, true, false});
    Check(result.Ok() && result.Value().state() == w::CONTROLLER_STATE_EMERGENCY_STOP && !f.backend->enabled,
          "independent operator stop failed");
    Session denied;
    auto denial = denied.session.Control(Request(), 1000, {});
    Check(denial.Ok() && denial.Value().code() == w::CONTROL_RESULT_CODE_REJECTED && !denial.Value().has_lease(),
          "session bypassed authorization");
}

void TestMalformed() {
    Session f; f.Arm(); Check(f.session.Command(f.Target(), 1002).Ok(), "target failed");
    auto malformed = f.Target(1050, 2); malformed.clear_context();
    Check(!f.session.Command(Wire(malformed), 1050).Ok() && f.session.Status().rejected_command_count == 1 &&
          f.session.Status().last_accepted_command_sequence == 1 && f.session.Status().last_accepted_command_time_ns == 1002,
          "malformed wire command refreshed state");
    auto management = Request(w::CONTROL_OPERATION_RENEW, 1102); management.clear_context();
    Check(!f.session.Control(management, 1102, {true, true, true}).Ok() &&
          f.session.Status().state == d::State::kFault && !f.backend->enabled, "malformed RPC bypassed watchdog");
    Session unknown; unknown.Arm(); auto target = unknown.Target(); Unknown(target.mutable_joints(0), "mode");
    Check(unknown.session.Command(Wire(target), 1002).reason == d::Reason::kUnsupportedMode, "unknown enum reached backend");
    Session expiring; expiring.Arm(); auto exp = expiring.Target(); exp.mutable_context()->set_expires_time_ns(1003);
    Check(expiring.session.Command(exp, 1002).Ok() && !expiring.session.Apply(1003).Ok() && expiring.backend->writes == 0,
          "queued wire command applied after expiration");
}

void TestLifecycle() {
    bool threw = false;
    try { hi::JointControlSession session(Caps(), nullptr, [] { return "token"; }); }
    catch (const std::invalid_argument&) { threw = true; }
    Check(threw, "null injected backend accepted");
    Session f; f.Arm(); const auto old = f.Target();
    Check(f.session.Command(old, 1002).Ok() && f.session.Apply(1002).Ok(), "target failed");
    Check(f.session.SetPaused(true, 1003).Ok() && !f.backend->enabled && !f.session.Status().owner_generation,
          "pause retained output or authority");
    Check(f.session.Reset("epoch-2"), "reset failed");
    Check(f.session.Command(old, 1004).reason == d::Reason::kWrongContext, "old wire command survived reset");
    Check(!f.session.Feedback(Frame()).Ok(), "old frame relabelled with new session");
    struct LifetimeBackend : Backend {
        bool& stopped_before_destruction;
        explicit LifetimeBackend(bool& result) : stopped_before_destruction(result) {}
        ~LifetimeBackend() override { stopped_before_destruction = !enabled && disables >= 2; }
    };
    bool destroyed_safely = false;
    { auto backend = std::make_unique<LifetimeBackend>(destroyed_safely);
      hi::JointControlSession owner(Caps(), std::move(backend), [] { return "token"; }); }
    Check(destroyed_safely, "backend destroyed before guard cleanup");
}

int main(int argc, char** argv) {
    try {
        Check(argc == 2, "case required"); const std::string which(argv[1]);
        if (which == "command") TestCommand(); else if (which == "request") TestRequest();
        else if (which == "capabilities") TestCapabilities(); else if (which == "result") TestResult();
        else if (which == "feedback") TestFeedback(); else if (which == "session") TestSession();
        else if (which == "malformed") TestMalformed(); else if (which == "lifecycle") TestLifecycle();
        else throw std::runtime_error("unknown case");
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
