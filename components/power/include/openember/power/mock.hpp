#pragma once
#include "openember/power/power.hpp"

namespace openember::power {
// Deterministic bench backend. Accepted/unknown outcomes do not update feedback.
class MockPowerDevice final : public IPowerDevice {
public:
    explicit MockPowerDevice(Capabilities capabilities);
    hardware::Result<void> Start() override;
    void Stop() noexcept override;
    Capabilities GetCapabilities() const override;
    hardware::Result<Snapshot> Read() override;
    ExecutionResult Execute(const Target&, std::uint64_t deadline_monotonic_ns) override;
    void SetNextOutcome(Outcome outcome) { next_ = outcome; }
    std::size_t ExecutionCount() const { return executions_; }
private:
    Capabilities capabilities_;
    Snapshot snapshot_;
    bool running_ = false;
    Outcome next_ = Outcome::kConfirmed;
    std::string domain_;
    std::size_t executions_ = 0;
};
} // namespace openember::power
