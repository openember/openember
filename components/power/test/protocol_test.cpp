#include "openember/power/protocol.hpp"
#include "openember/power/authority.hpp"
#include "openember/power/mock.hpp"
#include "openember/hardware/timestamp.hpp"
#include <iostream>
#include <stdexcept>

namespace p = openember::power;
namespace w = openember::msgs::power::v1;
void Check(bool b, const char* message) { if (!b) throw std::runtime_error(message); }
int main() {
    try {
        p::MockPowerDevice device({"pmu", {{"motor", p::ResourceKind::kChannel, true, true, false, true},
            {"fan", p::ResourceKind::kFan, true, false, true},
            {"light", p::ResourceKind::kLight, true, false, true}}, {"pack"}});
        device.Start(); p::Controller c(device, "current-session", "host-domain");
        bool ready = false;
        p::Context context{"authenticated-user", true,
            [&](const auto&, bool required, auto) -> p::GateResult {
                return required && !ready ? p::GateResult{p::Reason::kNotReady, "mock readiness absent"} : p::GateResult{};
            }};
        const auto now = openember::hardware::SteadyTimeNs();
        w::PowerCapabilities caps;
        Check(caps.ParseFromString(p::Describe(c).SerializeAsString()) && caps.resources_size() == 3 &&
            caps.resources(0).enable_requires_ready() && caps.resources(0).disable_requires_ready() && caps.request_capacity() == 256, "capability roundtrip");
        w::PowerControlRequest request;
        request.set_power_id("pmu"); request.set_service_session(caps.service_session());
        request.set_clock_domain(caps.clock_domain()); request.set_resource_id("motor");
        request.set_request_id("req"); request.set_deadline_monotonic_ns(now + 1000000000ULL);
        // A default protobuf bool must not silently become a switch-off command.
        Check(p::HandleControl(c, request, context, now).reason() == w::POWER_CONTROL_REASON_INVALID_REQUEST,
            "missing target became off");
        request.set_channel_enabled(true);
        Check(p::HandleControl(c, request, context, now).reason() == w::POWER_CONTROL_REASON_NOT_READY,
            "readiness bypass");
        ready = true;
        w::PowerControlRequest decoded;
        Check(decoded.ParseFromString(request.SerializeAsString()), "request decoding");
        auto result = p::HandleControl(c, decoded, context, now);
        w::PowerControlResult result_copy;
        Check(result_copy.ParseFromString(result.SerializeAsString()) &&
            result_copy.outcome() == w::POWER_CONTROL_OUTCOME_CONFIRMED &&
            result_copy.service_session() == caps.service_session(), "result roundtrip");
        p::HandleControl(c, decoded, context, now);
        Check(device.ExecutionCount() == 1, "wire duplicate executed");
        decoded.set_channel_enabled(false);
        Check(p::HandleControl(c, decoded, context, now).reason() == w::POWER_CONTROL_REASON_CONFLICT,
            "conflicting switch accepted");
        decoded.set_request_id("fan"); decoded.set_resource_id("fan"); decoded.set_fan_level(.3);
        Check(decoded.target_case() == w::PowerControlRequest::kFanLevel, "oneof target");
        device.SetNextOutcome(p::Outcome::kAccepted);
        Check(p::HandleControl(c, decoded, context, now).outcome() == w::POWER_CONTROL_OUTCOME_ACCEPTED,
            "ACK promoted to feedback");
        decoded.set_request_id("light"); decoded.set_resource_id("light"); decoded.set_light_level(0);
        Check(p::HandleControl(c, decoded, context, now).outcome() == w::POWER_CONTROL_OUTCOME_CONFIRMED,
            "explicit zero target missing");
        auto impostor = context; impostor.authenticated = false;
        Check(p::HandleControl(c, request, impostor, now).reason() == w::POWER_CONTROL_REASON_UNAUTHORIZED,
            "header granted authority");
        std::cout << "POWER_PROTOCOL_MOCK_OK\n";
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
