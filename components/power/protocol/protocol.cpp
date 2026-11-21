#include "openember/power/protocol.hpp"

namespace openember::power {
namespace wire = openember::msgs::power::v1;
namespace {
wire::PowerResourceKind Kind(ResourceKind k) {
    switch (k) {
    case ResourceKind::kChannel: return wire::POWER_RESOURCE_KIND_CHANNEL;
    case ResourceKind::kFan: return wire::POWER_RESOURCE_KIND_FAN;
    case ResourceKind::kLight: return wire::POWER_RESOURCE_KIND_LIGHT;
    default: return wire::POWER_RESOURCE_KIND_UNSPECIFIED;
    }
}
wire::PowerControlOutcome OutcomeValue(Outcome o) {
    switch (o) {
    case Outcome::kRejected: return wire::POWER_CONTROL_OUTCOME_REJECTED;
    case Outcome::kAccepted: return wire::POWER_CONTROL_OUTCOME_ACCEPTED;
    case Outcome::kConfirmed: return wire::POWER_CONTROL_OUTCOME_CONFIRMED;
    default: return wire::POWER_CONTROL_OUTCOME_UNKNOWN;
    }
}
wire::PowerControlReason ReasonValue(Reason r) {
    switch (r) {
    case Reason::kInvalidRequest: return wire::POWER_CONTROL_REASON_INVALID_REQUEST;
    case Reason::kUnauthorized: return wire::POWER_CONTROL_REASON_UNAUTHORIZED;
    case Reason::kWrongSession: return wire::POWER_CONTROL_REASON_WRONG_SESSION;
    case Reason::kExpired: return wire::POWER_CONTROL_REASON_EXPIRED;
    case Reason::kUnsupported: return wire::POWER_CONTROL_REASON_UNSUPPORTED;
    case Reason::kNotReady: return wire::POWER_CONTROL_REASON_NOT_READY;
    case Reason::kConflict: return wire::POWER_CONTROL_REASON_CONFLICT;
    case Reason::kBusy: return wire::POWER_CONTROL_REASON_BUSY;
    case Reason::kBackendFailure: return wire::POWER_CONTROL_REASON_BACKEND_FAILURE;
    default: return wire::POWER_CONTROL_REASON_UNSPECIFIED;
    }
}
}
wire::PowerControlResult HandleControl(Controller& controller, const wire::PowerControlRequest& input,
                                     const Context& context, std::uint64_t now) {
    Request q{input.request_id(), input.power_id(), input.service_session(), input.clock_domain(),
        input.deadline_monotonic_ns(), {input.resource_id(), ResourceKind::kUnspecified, 0}};
    switch (input.target_case()) {
    case wire::PowerControlRequest::kChannelEnabled:
        q.target.kind = ResourceKind::kChannel; q.target.level = input.channel_enabled() ? 1 : 0; break;
    case wire::PowerControlRequest::kFanLevel:
        q.target.kind = ResourceKind::kFan; q.target.level = input.fan_level(); break;
    case wire::PowerControlRequest::kLightLevel:
        q.target.kind = ResourceKind::kLight; q.target.level = input.light_level(); break;
    default: break; // Missing/future targets are rejected, never implicit off.
    }
    const auto result = controller.Handle(q, context, now);
    wire::PowerControlResult out;
    out.set_power_id(controller.GetCapabilities().power_id);
    out.set_service_session(controller.Session());
    out.set_request_id(result.request_id.substr(0, 127));
    out.set_outcome(OutcomeValue(result.outcome));
    out.set_reason(ReasonValue(result.reason)); out.set_message(result.message);
    return out;
}
wire::PowerCapabilities Describe(const Controller& c) {
    wire::PowerCapabilities out;
    out.set_power_id(c.GetCapabilities().power_id); out.set_service_session(c.Session());
    out.set_clock_domain(c.ClockDomain()); out.set_request_capacity(static_cast<std::uint32_t>(c.Capacity()));
    for (const auto& r : c.GetCapabilities().resources) {
        auto* resource = out.add_resources(); resource->set_resource_id(r.id); resource->set_kind(Kind(r.kind));
        resource->set_writable(r.writable); resource->set_enable_requires_ready(r.enable_requires_ready);
        resource->set_variable_level(r.variable_level);
        resource->set_disable_requires_ready(r.disable_requires_ready);
    }
    for (const auto& id : c.GetCapabilities().battery_ids) out.add_battery_ids(id);
    return out;
}
} // namespace openember::power
