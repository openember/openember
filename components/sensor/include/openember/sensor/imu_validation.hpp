#pragma once
#include "openember/sensor/imu.hpp"

namespace openember::sensor {
// Call with a clock in the SAME producer domain. Not a cross-host age check.
// No normalization, coordinate conversion, attitude estimation or safety policy.
bool IsFreshOrientation(const ImuSample& sample, const std::string& clock_domain,
                        std::uint64_t now_ns, std::uint64_t max_age_ns);
// Requires orientation + acceleration + angular velocity. Optional magnetic and
// temperature channels are checked for finite values only when marked valid.
bool IsFreshImu(const ImuSample& sample, const std::string& clock_domain,
                std::uint64_t now_ns, std::uint64_t max_age_ns);

// Single-threaded. Pins source/frame/mounting on first accepted sample.
// A new source/session requires an explicit new tracker. Rejected packets never
// refresh or replace the last good sample. Sequence zero is a legal first frame.
class ImuTracker {
public:
    explicit ImuTracker(std::string clock_domain);
    bool Accept(const ImuSample& sample, std::uint64_t now_ns, std::uint64_t max_age_ns);
    bool Fresh(std::uint64_t now_ns, std::uint64_t max_age_ns) const;
    const ImuSample& Sample() const { return sample_; }
private:
    std::string clock_domain_;
    ImuSample sample_;
};
} // namespace openember::sensor
