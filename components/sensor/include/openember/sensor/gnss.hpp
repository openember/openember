#pragma once

#include <chrono>
#include <cstdint>
#include <string>

#include "openember/hardware/result.hpp"
#include "openember/sensor/sensor.hpp"
#include "openember/sensor/types.hpp"

namespace openember::sensor {

enum class GnssFixType {
    kNoFix,
    kFix2D,
    kFix3D,
    kRtkFloat,
    kRtkFixed,
};

struct GnssFix {
    std::string sensor_id;
    std::string frame_id;
    std::uint64_t sample_time_monotonic_ns = 0;
    std::uint64_t sequence = 0;
    GnssFixType fix_type = GnssFixType::kNoFix;
    double latitude_deg = 0.0;
    double longitude_deg = 0.0;
    double altitude_m = 0.0;
    Vector3 velocity_enu_mps;
    std::uint32_t satellites_used = 0;
    double horizontal_accuracy_m = 0.0;
    double vertical_accuracy_m = 0.0;
};

class IGnss : public ISensor {
public:
    ~IGnss() override = default;

    virtual hardware::Result<GnssFix> ReadFix(
        std::chrono::milliseconds timeout) = 0;
};

}  // namespace openember::sensor
