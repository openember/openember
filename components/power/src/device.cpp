#include "openember/power/power.hpp"
#include "openember/hardware/timestamp.hpp"
namespace openember::power {
ExecutionResult IPowerDevice::ExecuteChecked(const Target& target, std::uint64_t deadline, ExecutionCheck check) {
    const auto now = hardware::SteadyTimeNs();
    if (now >= deadline) return {Outcome::kRejected, "request expired before dispatch", Reason::kExpired};
    if (!check) return {Outcome::kRejected, "execution authorization missing", Reason::kUnauthorized};
    try {
        const auto result = check(now);
        if (!result.Allowed()) return {Outcome::kRejected, result.message, result.reason};
    } catch (...) { return {Outcome::kRejected, "execution authorization unavailable", Reason::kNotReady}; }
    return Execute(target, deadline);
}
}
