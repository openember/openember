#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>
#include "openember/hardware/result.hpp"

namespace openember::power {

enum class ResourceKind { kUnspecified, kChannel, kFan, kLight };
enum class Outcome { kRejected, kAccepted, kConfirmed, kUnknown };
enum class Reason {
    kNone, kInvalidRequest, kUnauthorized, kWrongSession, kExpired,
    kUnsupported, kNotReady, kConflict, kBusy, kBackendFailure
};

struct ResourceCapability {
    std::string id;
    ResourceKind kind = ResourceKind::kUnspecified;
    bool writable = false;
    // Enabling an actuator supply requires independently established readiness.
    bool enable_requires_ready = false;
    // Fan/light target is normalized [0,1]; false permits only 0 or 1.
    bool variable_level = false;
    bool disable_requires_ready = false;
};
struct Capabilities {
    std::string power_id;
    std::vector<ResourceCapability> resources;
    std::vector<std::string> battery_ids;
};
struct Observation {
    std::uint64_t sequence = 0;
    std::uint64_t sample_time_monotonic_ns = 0; // Unknown is zero; never host receipt.
    std::uint64_t receive_time_monotonic_ns = 0;
    std::string clock_domain;
    bool receive_fresh = false; // Does not prove downstream BMS sample freshness.
};
struct BatteryState {
    std::string id;
    Observation observation;
    std::optional<bool> connected;
    std::optional<double> voltage_v, current_a, temperature_celsius, soc_percent;
};
struct ResourceState {
    std::string id;
    ResourceKind kind = ResourceKind::kUnspecified;
    Observation observation;
    std::optional<double> observed_level; // Unknown unless actually observed.
};
struct Snapshot {
    std::string power_id;
    std::vector<BatteryState> batteries;
    std::vector<ResourceState> resources;
};
struct Target {
    std::string resource_id;
    ResourceKind kind = ResourceKind::kUnspecified;
    double level = 0; // Channel: 0/1, fan/light: normalized [0,1].
};
struct GateResult {
    Reason reason = Reason::kNone;
    std::string message;
    bool Allowed() const { return reason == Reason::kNone; }
};
using ExecutionCheck = std::function<GateResult(std::uint64_t now_monotonic_ns)>;
struct ExecutionResult {
    Outcome outcome = Outcome::kUnknown;
    std::string message;
    Reason reason = Reason::kNone;
    // ACK/echo is not observation. Confirmed requires independent feedback.
};
class IPowerDevice {
public:
    virtual ~IPowerDevice() = default;
    virtual hardware::Result<void> Start() = 0;
    virtual void Stop() noexcept = 0;
    virtual Capabilities GetCapabilities() const = 0;
    virtual hardware::Result<Snapshot> Read() = 0;
    virtual ExecutionResult Execute(const Target&, std::uint64_t deadline_monotonic_ns) = 0;
    // Queued backends must override and run check immediately before first write.
    virtual ExecutionResult ExecuteChecked(const Target&, std::uint64_t deadline_monotonic_ns,
                                          ExecutionCheck check);
};

} // namespace openember::power
