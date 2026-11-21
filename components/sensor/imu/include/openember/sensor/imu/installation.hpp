#pragma once
#include "openember/sensor/imu/imu.hpp"
#include <array>
namespace openember::sensor {
// Explicit host-side declaration. No persistent device writes or calibration.
struct ImuInstallation {
    bool enabled=false;
    std::string sensor_frame,body_frame,reference_frame,revision;
    // v_body = R(q) v_sensor; named YAML representation is wxyz.
    std::array<double,4> sensor_to_body{1,0,0,0};
};
void ValidateImuInstallation(const ImuInstallation&);
// Input must follow the sensor-to-reference, Z-up convention declared by the
// installed device profile. Preserve original clocks/sequences and quality.
// Does not turn missing/invalid numeric fields into valid measurements.
hardware::Result<ImuSample> ApplyImuInstallation(const ImuSample&,const ImuInstallation&);
} // namespace openember::sensor
