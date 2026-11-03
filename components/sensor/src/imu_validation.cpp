#include "openember/sensor/imu_validation.hpp"
#include <cmath>
#include <stdexcept>
#include <utility>

namespace openember::sensor {
namespace {
bool Finite(const Vector3& v) { return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z); }
}
bool IsFreshOrientation(const ImuSample& sample, const std::string& domain,
                        std::uint64_t now, std::uint64_t max_age) {
    if (!sample.orientation_valid || domain.empty() || sample.clock_domain != domain ||
        sample.sensor_id.empty() || sample.frame_id.empty() || sample.reference_frame_id.empty() ||
        sample.mounting_revision.empty() || sample.receive_time_monotonic_ns == 0 ||
        sample.receive_time_monotonic_ns > now || max_age == 0 ||
        now - sample.receive_time_monotonic_ns > max_age) return false;
    double norm = 0;
    for (const double v : {sample.orientation.w, sample.orientation.x, sample.orientation.y, sample.orientation.z}) {
        if (!std::isfinite(v)) return false;
        norm += v * v;
    }
    return std::abs(norm - 1.0) <= 1e-6;
}
bool IsFreshImu(const ImuSample& sample, const std::string& domain,
                std::uint64_t now, std::uint64_t max_age) {
    return IsFreshOrientation(sample, domain, now, max_age) &&
        sample.acceleration_valid && Finite(sample.acceleration_mps2) &&
        sample.angular_velocity_valid && Finite(sample.angular_velocity_radps) &&
        (!sample.magnetic_field_valid || Finite(sample.magnetic_field_tesla)) &&
        (!sample.temperature_valid || std::isfinite(sample.temperature_celsius));
}
ImuTracker::ImuTracker(std::string domain) : clock_domain_(std::move(domain)) {
    if (clock_domain_.empty()) throw std::invalid_argument("IMU clock domain required");
}
bool ImuTracker::Accept(const ImuSample& sample, std::uint64_t now, std::uint64_t max_age) {
    if (!IsFreshImu(sample, clock_domain_, now, max_age)) return false;
    if (sample_.orientation_valid &&
        (sample.sequence <= sample_.sequence || sample.receive_time_monotonic_ns <= sample_.receive_time_monotonic_ns ||
         sample.sensor_id != sample_.sensor_id || sample.frame_id != sample_.frame_id ||
         sample.reference_frame_id != sample_.reference_frame_id || sample.mounting_revision != sample_.mounting_revision))
        return false;
    sample_ = sample;
    return true;
}
bool ImuTracker::Fresh(std::uint64_t now, std::uint64_t max_age) const {
    return IsFreshImu(sample_, clock_domain_, now, max_age);
}
} // namespace openember::sensor
