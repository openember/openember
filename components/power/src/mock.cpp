#include "openember/power/mock.hpp"
#include <algorithm>
#include <atomic>
#include <cmath>
#include "openember/hardware/timestamp.hpp"

namespace openember::power {
MockPowerDevice::MockPowerDevice(Capabilities capabilities) : capabilities_(std::move(capabilities)) {
    snapshot_.power_id = capabilities_.power_id;
    for (const auto& r : capabilities_.resources)
        snapshot_.resources.push_back({r.id, r.kind, {}, std::nullopt});
    for (const auto& id : capabilities_.battery_ids) {
        BatteryState b; b.id = id; snapshot_.batteries.push_back(b);
    }
}
hardware::Result<void> MockPowerDevice::Start() {
    if (running_) return hardware::Result<void>::Success();
    static std::atomic<std::uint64_t> sequence{0};
    domain_ = "mock-power-" + std::to_string(hardware::SteadyTimeNs()) + "-" + std::to_string(++sequence);
    for (auto& r : snapshot_.resources) { r.observation = {}; r.observed_level.reset(); }
    running_ = true;
    return hardware::Result<void>::Success();
}
void MockPowerDevice::Stop() noexcept {
    running_ = false;
    for (auto& r : snapshot_.resources) r.observation.receive_fresh = false;
}
Capabilities MockPowerDevice::GetCapabilities() const { return capabilities_; }
hardware::Result<Snapshot> MockPowerDevice::Read() {
    if (!running_) return hardware::Result<Snapshot>::Failure(
        hardware::MakeError(hardware::ErrorCode::kNotRunning, "mock power device is stopped"));
    auto out = snapshot_;
    const auto now = hardware::SteadyTimeNs();
    for (auto& r : out.resources) {
        const auto received = r.observation.receive_time_monotonic_ns;
        r.observation.receive_fresh = received && now >= received && now - received <= 1000000000ULL;
    }
    return hardware::Result<Snapshot>::Success(std::move(out));
}
ExecutionResult MockPowerDevice::Execute(const Target& target, std::uint64_t deadline) {
    if (!running_ || hardware::SteadyTimeNs() >= deadline)
        return {Outcome::kRejected, "mock power device stopped or deadline elapsed"};
    const auto r = std::find_if(capabilities_.resources.begin(), capabilities_.resources.end(),
        [&](const auto& c) { return c.id == target.resource_id; });
    if (r == capabilities_.resources.end() || !r->writable || r->kind != target.kind ||
        !std::isfinite(target.level) || target.level < 0 || target.level > 1 ||
        ((!r->variable_level || r->kind == ResourceKind::kChannel) && target.level != 0 && target.level != 1))
        return {Outcome::kRejected, "invalid mock operation"};
    ++executions_;
    const auto outcome = next_; next_ = Outcome::kConfirmed;
    if (outcome == Outcome::kConfirmed) {
        auto& state = snapshot_.resources[static_cast<std::size_t>(r - capabilities_.resources.begin())];
        state.observed_level = target.level;
        ++state.observation.sequence;
        state.observation.sample_time_monotonic_ns = state.observation.receive_time_monotonic_ns = hardware::SteadyTimeNs();
        state.observation.clock_domain = domain_;
        // Read() is a retained snapshot; consumer must assess its age.
        state.observation.receive_fresh = true;
    }
    return {outcome, "mock execution"};
}
} // namespace openember::power
