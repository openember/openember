#pragma once

#include <string>

namespace openember::actuator {

enum class ActuatorType {
    kUnspecified,
    kJointController,
};

enum class ActuatorBusType {
    kUnspecified,
    kVirtual,
    kSerial,
    kCan,
    kEtherCat,
    kPwm,
    kGpio,
};

struct ActuatorInfo {
    std::string actuator_id;
    std::string name;
    ActuatorType type = ActuatorType::kUnspecified;
    std::string vendor;
    std::string model;
    std::string driver;
    ActuatorBusType bus_type = ActuatorBusType::kUnspecified;
};

}  // namespace openember::actuator
