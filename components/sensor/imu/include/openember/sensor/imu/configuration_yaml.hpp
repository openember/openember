#pragma once
#include "openember/sensor/imu_driver.hpp"
namespace openember::sensor {
// Reads one enabled IMU from schema_version: 1 / devices; no hardware is opened.
ImuDriverConfig LoadImuDriverConfig(const std::string& path, const std::string& sensor_id);
}
