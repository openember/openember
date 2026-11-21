#include "openember/power/controller.hpp"
#include "openember/power/authority.hpp"
#include "openember/power/mock.hpp"
#include "openember/hardware/timestamp.hpp"
#include <iostream>
#include <limits>
#include <stdexcept>

namespace p = openember::power;
void Check(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
p::Capabilities Caps() {
    return {"pmu", {{"motor", p::ResourceKind::kChannel, true, true, false},
        {"fan", p::ResourceKind::kFan, true, false, true},
        {"lamp", p::ResourceKind::kLight, false, false, false}}, {"battery"}};
}
// Explicitly model a write followed by an exception, not a clean rejection.
class AmbiguousDevice final : public p::IPowerDevice {
public:
    int writes = 0;
    openember::hardware::Result<void> Start() override { return openember::hardware::Result<void>::Success(); }
    void Stop() noexcept override {}
    p::Capabilities GetCapabilities() const override { return Caps(); }
    openember::hardware::Result<p::Snapshot> Read() override { return openember::hardware::Result<p::Snapshot>::Success({}); }
    p::ExecutionResult Execute(const p::Target&, std::uint64_t) override {
        ++writes; throw std::runtime_error("response lost after write");
    }
};
int main() {
    try {
        p::MockPowerDevice device(Caps());
        Check(!device.Read().Ok(), "stopped device accepted read");
        Check(device.Start().Ok(), "start failed");
        Check(!device.Read().Value().resources[0].observed_level, "unknown state became off");
        p::Controller controller(device, "session", "host", 3);
        bool ready = false;
        p::Authority authority("robot", "pmu", "session", "host");
        authority.SetPermissions("operator", {{"motor", p::Operation::kEnable}, {"motor", p::Operation::kDisable},
            {"fan", p::Operation::kSetLevel}, {"lamp", p::Operation::kSetLevel}, {"absent", p::Operation::kEnable}});
        auto context = authority.ForAuthenticated("operator");
        const auto policy = context.authorize;
        context.authorize = [&](const auto& t, bool needs_ready, auto time) -> p::GateResult {
            const auto permission = policy(t, false, time);
            if (!permission.Allowed()) return permission;
            if (needs_ready && !ready) return {p::Reason::kNotReady, "mock readiness absent"};
            return {};
        };
        const auto now = openember::hardware::SteadyTimeNs();
        p::Request q{"1", "pmu", "session", "host", now + 10000000000ULL,
            {"motor", p::ResourceKind::kChannel, 1}};
        auto bad = context; bad.authenticated = false;
        Check(controller.Handle(q, bad, now).reason == p::Reason::kUnauthorized, "untrusted caller accepted");
        bad = context; bad.authorize = {};
        Check(controller.Handle(q, bad, now).reason == p::Reason::kUnauthorized, "missing permission accepted");
        auto invalid = q; invalid.service_session = "old";
        Check(controller.Handle(invalid, context, now).reason == p::Reason::kWrongSession, "old session accepted");
        invalid = q; invalid.clock_domain = "other-host";
        Check(controller.Handle(invalid, context, now).reason == p::Reason::kWrongSession, "foreign clock accepted");
        invalid = q; invalid.deadline_monotonic_ns = now;
        Check(controller.Handle(invalid, context, now).reason == p::Reason::kExpired, "expired request accepted");
        invalid = q; invalid.target.level = std::numeric_limits<double>::quiet_NaN();
        Check(controller.Handle(invalid, context, now).reason == p::Reason::kInvalidRequest, "NaN accepted");
        invalid = q; invalid.request_id = std::string("id\0suffix", 9);
        Check(controller.Handle(invalid, context, now).reason == p::Reason::kInvalidRequest, "embedded NUL ID accepted");
        invalid = q; invalid.target.kind = static_cast<p::ResourceKind>(999);
        Check(controller.Handle(invalid, context, now).reason == p::Reason::kInvalidRequest, "future operation accepted");
        invalid = q; invalid.power_id = "other";
        Check(controller.Handle(invalid, context, now).reason == p::Reason::kUnsupported, "wrong device accepted");
        invalid = q; invalid.target.level = .5;
        Check(controller.Handle(invalid, context, now).reason == p::Reason::kInvalidRequest, "fractional switch accepted");
        invalid = q; invalid.target.resource_id = "absent";
        Check(controller.Handle(invalid, context, now).reason == p::Reason::kUnsupported, "unknown resource accepted");
        invalid = q; invalid.target = {"lamp", p::ResourceKind::kLight, 1};
        Check(controller.Handle(invalid, context, now).reason == p::Reason::kUnsupported, "readonly resource accepted");
        Check(controller.Handle(q, context, now).reason == p::Reason::kNotReady, "enabled before readiness");
        Check(device.ExecutionCount() == 0, "invalid requests touched device");
        ready = true;
        Check(controller.Handle(q, context, now).outcome == p::Outcome::kConfirmed, "enable failed");
        ready = false;
        Check(controller.Handle(q, context, now).outcome == p::Outcome::kConfirmed, "duplicate lost original result");
        Check(device.ExecutionCount() == 1, "duplicate executed twice");
        bad = context; bad.authorize = {};
        Check(controller.Handle(q, bad, now).reason == p::Reason::kUnauthorized, "cached result bypassed revoked permission");
        auto state = device.Read().Value().resources[0];
        Check(state.observed_level == 1 && state.observation.sequence == 1, "confirmed feedback absent");
        invalid = q; invalid.target.level = 0;
        Check(controller.Handle(invalid, context, now).reason == p::Reason::kConflict, "ID content conflict accepted");
        invalid = q; invalid.deadline_monotonic_ns += 1;
        Check(controller.Handle(invalid, context, q.deadline_monotonic_ns).reason == p::Reason::kConflict,
            "expired ID could be recycled with new deadline");
        q.request_id = "2"; q.target.level = 0;
        device.SetNextOutcome(p::Outcome::kAccepted);
        Check(controller.Handle(q, context, now).outcome == p::Outcome::kAccepted, "acceptance promoted to confirmation");
        Check(device.Read().Value().resources[0].observed_level == 1, "ACK changed observed feedback");
        Check(controller.Handle(q, context, now).outcome == p::Outcome::kAccepted, "accepted request retried");
        q.request_id = "3"; q.target = {"fan", p::ResourceKind::kFan, .4};
        device.SetNextOutcome(p::Outcome::kUnknown);
        Check(controller.Handle(q, context, now).outcome == p::Outcome::kUnknown, "unknown result lost");
        Check(controller.Handle(q, context, now).outcome == p::Outcome::kUnknown && device.ExecutionCount() == 3,
            "unknown result retried");
        q.request_id = "4";
        Check(controller.Handle(q, context, now).reason == p::Reason::kBusy, "full history evicted old requests");
        Check(controller.Handle(q, context, q.deadline_monotonic_ns).reason == p::Reason::kExpired, "expired request executed");
        device.Stop(); Check(!device.Read().Ok(), "read after stop");
        device.Start();
        Check(!device.Read().Value().resources[0].observed_level, "restart restored old intent");
        AmbiguousDevice ambiguous; p::Controller uncertain(ambiguous, "session", "host");
        Check(uncertain.Handle(q, context, now).outcome == p::Outcome::kUnknown, "exception treated as safe rejection");
        Check(uncertain.Handle(q, context, now).outcome == p::Outcome::kUnknown && ambiguous.writes == 1,
            "write-after-exception retried");
        bool rejected = false;
        auto duplicate = Caps(); duplicate.resources.push_back(duplicate.resources[0]);
        try { p::MockPowerDevice d(duplicate); p::Controller c(d, "s", "c"); }
        catch (const std::invalid_argument&) { rejected = true; }
        Check(rejected, "duplicate resource capabilities accepted");
        std::cout << "POWER_CONTROLLER_OK\n";
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
