#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <vector>
#include "openember/hardware/result.hpp"

namespace openember::sensor {
enum class ImuOutputField : std::uint32_t {
    kAcceleration = 1, kAngularVelocity = 2, kOrientation = 4,
    kTemperature = 8, kMagneticField = 16, kEulerAngles = 32, kDeviceTime = 64
};
using ImuOutputFields = std::uint32_t;
constexpr ImuOutputFields Field(ImuOutputField field) { return static_cast<ImuOutputFields>(field); }
enum class ImuPersistence { kRam, kFlash };
enum class ImuSettingState { kApplied, kRejected, kUnconfirmed };
struct ImuCapabilities {
    bool configuration_access = false;
    std::vector<double> output_rates_hz;
    std::vector<std::uint32_t> baud_rates;
    ImuOutputFields output_fields = 0;
    std::vector<ImuPersistence> persistence;
    bool static_gyro_calibration = false;
    std::string profile;
};
struct ImuDeviceConfiguration {
    std::optional<double> output_rate_hz;
    std::optional<std::uint32_t> baud_rate;
    std::optional<ImuOutputFields> output_fields;
    std::uint32_t host_baud_rate = 0;
    std::uint64_t queried_at_monotonic_ns = 0;
    // Persistence cannot be inferred from reading current RAM values.
};
struct ImuConfigurationPatch {
    std::optional<double> output_rate_hz;
    std::optional<std::uint32_t> baud_rate;
    std::optional<ImuOutputFields> output_fields;
    ImuPersistence persistence = ImuPersistence::kRam;
};
struct ImuSettingResult {
    std::string setting;
    ImuSettingState state = ImuSettingState::kUnconfirmed;
    std::string message;
};
struct ImuConfigurationResult {
    std::vector<ImuSettingResult> settings;
    std::optional<ImuDeviceConfiguration> observed;
    // Earlier successful fields are retained if a later field fails. No implicit rollback.
};
class IImuConfiguration {
public:
    virtual ~IImuConfiguration() = default;
    virtual ImuCapabilities Capabilities() const = 0;
    virtual hardware::Result<ImuDeviceConfiguration> ReadConfiguration() = 0;
    virtual hardware::Result<ImuConfigurationResult> ApplyConfiguration(const ImuConfigurationPatch&) = 0;
};
} // namespace openember::sensor
