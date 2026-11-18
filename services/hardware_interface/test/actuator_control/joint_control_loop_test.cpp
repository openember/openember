#include "openember/services/hardware_interface/joint_control_loop.hpp"
#include "openember/hardware/timestamp.hpp"

#include <atomic>
#include <condition_variable>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <thread>

namespace hi = openember::services::hardware_interface;
namespace d = hi::actuator_control::domain;
namespace w = hi::actuator_control::wire;
using Mode = openember::actuator::JointControlMode;
using namespace std::chrono_literals;
void Check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
std::uint64_t Now() { return openember::hardware::SteadyTimeNs(); }
template <typename F> void Until(F condition) {
    const auto deadline = std::chrono::steady_clock::now() + 2s;
    while (!condition()) { Check(std::chrono::steady_clock::now() < deadline, "condition timed out"); std::this_thread::sleep_for(1ms); }
}
template <typename T> T Await(std::future<T> future) {
    Check(future.wait_for(2s) == std::future_status::ready, "future was not completed"); return future.get();
}
struct Gate {
    std::mutex mutex; std::condition_variable cv;
    bool closed = false, entered = false;
    void Close() { std::lock_guard<std::mutex> lock(mutex); closed = true; entered = false; }
    void Open() { std::lock_guard<std::mutex> lock(mutex); closed = false; cv.notify_all(); }
    void Wait() {
        std::unique_lock<std::mutex> lock(mutex);
        if (closed) { entered = true; cv.notify_all(); cv.wait_for(lock, 2s, [&] { return !closed; }); }
    }
    void Entered() { std::unique_lock<std::mutex> lock(mutex); Check(cv.wait_for(lock, 2s, [&] { return entered; }), "reader did not block"); }
};
struct Stats {
    std::thread::id owner;
    std::atomic<int> writes{0}, disables{0}, destroyed{0};
    std::atomic<bool> wrong_thread{false}, fail_disable{false}, fail_read{false}, freeze{false};
    Gate gate;
};
struct Backend : d::Backend {
    std::shared_ptr<Stats> stats;
    explicit Backend(std::shared_ptr<Stats> s) : stats(std::move(s)) { stats->owner = std::this_thread::get_id(); }
    void Thread() { if (stats->owner != std::this_thread::get_id()) stats->wrong_thread.store(true); }
    ~Backend() override { Thread(); ++stats->destroyed; }
    bool Ready() noexcept override { Thread(); return true; }
    bool Apply(const std::vector<d::Target>&) noexcept override { Thread(); ++stats->writes; return true; }
    bool Disable() noexcept override { Thread(); ++stats->disables; return !stats->fail_disable.load(); }
    bool EmergencyStop() noexcept override { return Disable(); }
    bool ClearFault() noexcept override { Thread(); return true; }
};
d::Capabilities Caps() {
    d::Capabilities c; c.contract_version = 2; c.robot_id = "test-sim"; c.controller_id = "joints";
    c.controller_session = "epoch-1"; c.model_revision = "fixture"; c.clock_domain = "host-monotonic:test-boot";
    c.simulation = true; c.max_command_duration_ns = 2000000000; c.max_lease_duration_ns = 10000000000;
    c.command_timeout_ns = 500000000;
    d::JointCapability j; j.joint_name = "joint"; j.min_position_rad = -1; j.max_position_rad = 1;
    j.max_velocity_radps = 10; j.max_effort_nm = 10; j.max_stiffness_nm_per_rad = 100; j.max_damping_nms_per_rad = 10;
    j.max_feedback_age_ns = 20000000; j.supported_modes = {Mode::kMit}; c.joints = {j}; return c;
}
w::CommandContext Context(std::uint64_t sequence) {
    const auto c = Caps(); const auto now = Now(); w::CommandContext out;
    out.set_robot_id(c.robot_id); out.set_controller_id(c.controller_id); out.set_controller_session(c.controller_session);
    out.set_model_revision(c.model_revision); out.set_clock_domain(c.clock_domain); out.set_producer_session("test");
    out.set_sequence(sequence); out.set_issued_time_ns(now); out.set_expires_time_ns(now + 1000000000); return out;
}
w::ControlRequest Request(w::ControlOperation op, std::uint64_t sequence = 1) {
    w::ControlRequest out; *out.mutable_context() = Context(sequence);
    out.set_request_id("request-" + std::to_string(op) + "-" + std::to_string(sequence)); out.set_operation(op);
    if (op == w::CONTROL_OPERATION_ACQUIRE) out.set_requested_lease_duration_ns(5000000000);
    return out;
}
hi::JointControlLoopOptions Options(const std::shared_ptr<Stats>& stats) {
    hi::JointControlLoopOptions options; options.capabilities = Caps(); options.period = 2ms;
    options.clock = Now; options.tokens = [] { return "test-only-token"; };
    options.backend = [stats] { return std::make_unique<Backend>(stats); };
    options.read = [stats, step = std::uint64_t{0}, received = std::uint64_t{0}]() mutable {
        stats->gate.Wait();
        if (stats->fail_read.load()) throw std::runtime_error("injected reader failure");
        if (!stats->freeze.load()) { ++step; received = Now(); }
        d::FeedbackFrame frame; frame.controller_session = Caps().controller_session; frame.clock_domain = Caps().clock_domain;
        frame.physics_step = step; frame.simulation_time_ns = step * 2000000;
        d::JointFeedback j; j.joint_name = "joint"; j.sample_sequence = step; j.receive_time_ns = received;
        j.receive_fresh = true; j.position_valid = j.velocity_valid = true; frame.joints = {j}; return frame;
    };
    return options;
}
struct Fixture {
    std::shared_ptr<Stats> stats = std::make_shared<Stats>();
    hi::JointControlLoop loop{Options(stats)};
    w::ControlLease lease;
    void Start() { Check(loop.Start(), "worker did not start"); }
    void Arm() {
        Start(); auto acquired = Await(loop.Control(Request(w::CONTROL_OPERATION_ACQUIRE), {true, true, true}));
        Check(acquired.Ok() && acquired.Value().code() == w::CONTROL_RESULT_CODE_COMPLETED, "acquire failed"); lease = acquired.Value().lease();
        auto request = Request(w::CONTROL_OPERATION_ARM); *request.mutable_lease() = lease;
        const auto result = Await(loop.Control(request, {true, true, true}));
        Check(result.Ok() && result.Value().code() == w::CONTROL_RESULT_CODE_COMPLETED, "arm failed");
    }
    w::JointCommand Command(std::uint64_t sequence = 1) const {
        w::JointCommand out; *out.mutable_context() = Context(sequence); *out.mutable_lease() = lease;
        if (!out.lease().generation()) { out.mutable_lease()->set_generation(1); out.mutable_lease()->set_token("no-owner"); }
        auto* j = out.add_joints(); j->set_joint_name("joint"); j->set_mode(w::JOINT_CONTROL_MODE_MIT);
        j->set_max_effort_nm(5); j->set_stiffness_nm_per_rad(20); j->set_damping_nms_per_rad(1); return out;
    }
};
void Lifecycle() {
    Fixture f; f.Arm(); Check(Await(f.loop.Command(f.Command())).Ok(), "target rejected");
    Until([&] { return f.stats->writes.load() > 0; });
    f.loop.Stop(); Check(!f.loop.Running() && f.loop.Status().state == d::State::kStopped &&
        f.stats->destroyed == 1 && !f.stats->wrong_thread.load() && f.stats->owner != std::this_thread::get_id(), "ownership or stop failure");
    Check(!f.loop.Start(), "terminal worker restarted");
    Check(!Await(f.loop.Command(f.Command(2))).Ok(), "stopped worker accepted command");
    const auto latest = f.loop.Latest(); Check(latest && latest->state() == w::CONTROLLER_STATE_STOPPED, "stale armed feedback after stop");
}
void Priority() {
    Fixture f; f.Arm(); f.stats->gate.Close(); f.stats->gate.Entered();
    auto clear = f.loop.Control(Request(w::CONTROL_OPERATION_CLEAR_FAULT), {false, false, true});
    std::vector<std::future<d::Decision>> targets;
    for (int i = 1; i <= 31; ++i) targets.push_back(f.loop.Command(f.Command(i)));
    Check(Await(f.loop.Command(f.Command(32))).reason == d::Reason::kBusy, "ordinary queue was not bounded");
    auto stop = f.loop.Control(Request(w::CONTROL_OPERATION_EMERGENCY_STOP), {false, true, false});
    f.stats->gate.Open(); const auto result = Await(std::move(stop));
    Check(result.Ok() && result.Value().state() == w::CONTROLLER_STATE_EMERGENCY_STOP, "priority stop was lost");
    Check(!Await(std::move(clear)).Ok(), "queued clear survived priority stop");
    for (auto& future : targets) Check(!Await(std::move(future)).Ok(), "queued target overtook stop");
    Check(f.stats->writes == 0, "target written after priority stop"); f.loop.Stop();
}
void Overflow() {
    Fixture f; f.Start(); f.stats->gate.Close(); f.stats->gate.Entered();
    auto invalid = Request(w::CONTROL_OPERATION_EMERGENCY_STOP); invalid.mutable_context()->set_controller_session("old");
    Check(!Await(f.loop.Control(invalid, {false, true, false})).Ok() && f.loop.Running(), "old stop caused shutdown");
    std::vector<std::future<hi::actuator_control::Result<w::ControlResult>>> stops;
    for (int i = 1; i <= 8; ++i) stops.push_back(f.loop.Control(Request(w::CONTROL_OPERATION_EMERGENCY_STOP, i), {false, true, false}));
    Check(!Await(f.loop.Control(Request(w::CONTROL_OPERATION_EMERGENCY_STOP, 9), {false, true, false})).Ok(), "priority queue unbounded");
    f.stats->gate.Open(); f.loop.Stop();
    for (auto& future : stops) Check(!Await(std::move(future)).Ok(), "overflow announced a completed operation");
    Check(f.loop.Status().state == d::State::kStopped && f.stats->disables > 1, "overflow did not stop backend");
}
void Expiry() {
    Fixture f; f.Arm(); f.stats->gate.Close(); f.stats->gate.Entered();
    auto command = f.Command(); command.mutable_context()->set_expires_time_ns(command.context().issued_time_ns() + 1000000);
    auto pending = f.loop.Command(command); std::this_thread::sleep_for(5ms); f.stats->gate.Open();
    Check(Await(std::move(pending)).reason == d::Reason::kExpired && f.stats->writes == 0, "queue residence did not expire target");
    f.loop.RequestStop(); f.loop.Stop();
}
void Feedback() {
    Fixture f; f.Arm(); Check(Await(f.loop.Command(f.Command())).Ok(), "target rejected");
    Until([&] { return f.stats->writes.load() > 0; }); f.stats->freeze.store(true);
    Until([&] { return f.loop.Status().state == d::State::kFault; });
    const auto count = f.stats->writes.load(); std::this_thread::sleep_for(10ms);
    Check(f.stats->writes == count && f.loop.Status().fault_reason == d::Reason::kExpired, "stale feedback did not interlock");
    f.loop.Stop();
}
void Exception() {
    Fixture startup; startup.stats->fail_disable.store(true);
    Check(!startup.loop.Start() && !startup.loop.Error().empty(), "startup disable failure ignored"); startup.loop.Stop();
    Fixture runtime; runtime.Arm(); runtime.stats->fail_read.store(true);
    Until([&] { return !runtime.loop.Running(); }); runtime.loop.Stop();
    Check(!runtime.loop.Error().empty() && runtime.loop.Status().state == d::State::kStopped && runtime.stats->destroyed == 1,
          "reader exception did not stop and destroy backend");
    auto stats = std::make_shared<Stats>(); auto options = Options(stats); auto fail_clock = std::make_shared<std::atomic<bool>>(false);
    options.clock = [fail_clock] { if (fail_clock->load()) throw std::runtime_error("clock failure"); return Now(); };
    hi::JointControlLoop clock(std::move(options)); Check(clock.Start(), "clock test did not start"); fail_clock->store(true);
    Check(!Await(clock.Control(Request(w::CONTROL_OPERATION_EMERGENCY_STOP), {false, true, false})).Ok(), "clock failure escaped as completion");
    clock.Stop(); Check(clock.Status().state == d::State::kStopped && !clock.Error().empty(), "clock failure did not stop");
}
void Concurrent() {
    Fixture f; f.Start(); std::atomic<int> done{0}; std::vector<std::thread> producers;
    for (int p = 0; p < 4; ++p) producers.emplace_back([&] {
        std::vector<std::future<d::Decision>> pending;
        for (int i = 1; i <= 40; ++i) { pending.push_back(f.loop.Command(f.Command(i))); f.loop.Status(); f.loop.Latest(); }
        for (auto& future : pending) if (!Await(std::move(future)).Ok()) ++done;
    });
    for (auto& producer : producers) producer.join();
    f.loop.Stop(); Check(done == 160 && !f.stats->wrong_thread.load(), "concurrent ingress lost completion or worker ownership");
}
int main(int argc, char** argv) {
    try {
        Check(argc == 2, "case required"); const std::string which(argv[1]);
        if (which == "lifecycle") Lifecycle(); else if (which == "priority") Priority();
        else if (which == "overflow") Overflow(); else if (which == "expiry") Expiry();
        else if (which == "feedback") Feedback(); else if (which == "exception") Exception();
        else if (which == "concurrent") Concurrent(); else throw std::runtime_error("unknown case");
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
