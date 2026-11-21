#include "openember/sensor/imu/installation.hpp"
#include <cmath>
#include <stdexcept>
namespace openember::sensor {
namespace {
using Q=std::array<double,4>;
double Norm2(Q q) { double v=0; for (auto x:q) v+=x*x; return v; }
Q Multiply(Q a,Q b) {
    return {a[0]*b[0]-a[1]*b[1]-a[2]*b[2]-a[3]*b[3],a[0]*b[1]+a[1]*b[0]+a[2]*b[3]-a[3]*b[2],
            a[0]*b[2]-a[1]*b[3]+a[2]*b[0]+a[3]*b[1],a[0]*b[3]+a[1]*b[2]-a[2]*b[1]+a[3]*b[0]};
}
Q Inverse(Q q) { return {q[0],-q[1],-q[2],-q[3]}; }
bool Finite(Vector3 v) { return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z); }
Vector3 Rotate(Q q,Vector3 v) { const auto a=Multiply(Multiply(q,{0,v.x,v.y,v.z}),Inverse(q)); return {a[1],a[2],a[3]}; }
}
void ValidateImuInstallation(const ImuInstallation& c) {
    if (!c.enabled) return;
    const auto n=Norm2(c.sensor_to_body);
    if (c.sensor_frame.empty() || c.body_frame.empty() || c.reference_frame.empty() || c.revision.empty() || c.revision=="unverified" ||
        c.body_frame==c.reference_frame || !std::isfinite(n) || std::abs(n-1)>1e-9)
        throw std::invalid_argument("invalid IMU installation identity or unit rotation");
    if (c.sensor_frame==c.body_frame && std::abs(c.sensor_to_body[0])<1-1e-12)
        throw std::invalid_argument("nonidentity IMU installation requires distinct sensor/body frames");
}
hardware::Result<ImuSample> ApplyImuInstallation(const ImuSample& raw,const ImuInstallation& c) {
    using Result=hardware::Result<ImuSample>;
    try {
        ValidateImuInstallation(c); if (!c.enabled) return Result::Success(raw);
        if (raw.frame_id!=c.sensor_frame || raw.clock_domain.empty() || !raw.sequence || !raw.receive_time_monotonic_ns)
            throw std::runtime_error("IMU installation sample identity/time mismatch");
        auto out=raw; out.frame_id=c.body_frame; out.reference_frame_id=c.reference_frame; out.mounting_revision=c.revision;
        if (raw.acceleration_valid) { if (!Finite(raw.acceleration_mps2)) throw std::runtime_error("invalid IMU acceleration"); out.acceleration_mps2=Rotate(c.sensor_to_body,raw.acceleration_mps2); }
        if (raw.angular_velocity_valid) { if (!Finite(raw.angular_velocity_radps)) throw std::runtime_error("invalid IMU angular velocity"); out.angular_velocity_radps=Rotate(c.sensor_to_body,raw.angular_velocity_radps); }
        if (raw.magnetic_field_valid) { if (!Finite(raw.magnetic_field_tesla)) throw std::runtime_error("invalid IMU magnetic field"); out.magnetic_field_tesla=Rotate(c.sensor_to_body,raw.magnetic_field_tesla); }
        out.orientation_valid=false;
        if (raw.orientation_present || raw.orientation_valid) {
            Q q{raw.orientation.w,raw.orientation.x,raw.orientation.y,raw.orientation.z}; const auto n=Norm2(q);
            if (!std::isfinite(n) || std::abs(n-1)>1e-5) throw std::runtime_error("invalid IMU installation quaternion");
            // Normalize only admissible numerical quantization, never arbitrary data.
            for (auto& v:q) v/=std::sqrt(n);
            q=Multiply(q,Inverse(c.sensor_to_body)); // R_reference_body = R_reference_sensor R_sensor_body
            out.orientation={q[0],q[1],q[2],q[3]}; out.orientation_present=out.orientation_valid=true;
        }
        return Result::Success(std::move(out));
    } catch (const std::exception& e) { return Result::Failure(hardware::MakeError(hardware::ErrorCode::kInvalidConfig,e.what())); }
}
} // namespace openember::sensor
