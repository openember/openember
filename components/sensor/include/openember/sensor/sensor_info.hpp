#pragma once

#include <string>

namespace openember::sensor {

enum class SensorType {
    kUnspecified,
    kImu,
    kTemperature,
    kGnss,
};

enum class SensorBusType {
    kUnspecified,
    kVirtual,
    kSerial,
    kI2c,
    kSpi,
    kCan,
    kGpio,
    kOneWire,
};

enum class SensorFetchMode {
    kUnspecified,
    kPolling,
    kEvent,
    kFifo,
};

struct SensorInfo {
    std::string sensor_id;
    std::string name;
    SensorType type = SensorType::kUnspecified;
    std::string vendor;
    std::string model;
    std::string driver;
    std::string frame_id;
    SensorBusType bus_type = SensorBusType::kUnspecified;
    SensorFetchMode fetch_mode = SensorFetchMode::kUnspecified;
    double min_period_ms = 0.0;
};

}  // namespace openember::sensor
