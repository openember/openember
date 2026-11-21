#pragma once
#include "openember/sensor/imu/configuration.hpp"
namespace openember::sensor {
struct ImuCalibrationRequest {
    ImuPersistence persistence = ImuPersistence::kRam;
};
enum class ImuCalibrationState { kAccepted, kCompleted, kRejected, kUnconfirmed };
struct ImuCalibrationResult {
    ImuCalibrationState state = ImuCalibrationState::kUnconfirmed;
    std::string message;
};
class IImuCalibration {
public:
    virtual ~IImuCalibration() = default;
    virtual hardware::Result<ImuCalibrationResult> CalibrateStaticGyro(const ImuCalibrationRequest&) = 0;
};
} // namespace openember::sensor
