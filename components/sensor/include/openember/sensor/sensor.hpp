#pragma once

#include "openember/hardware/device_status.hpp"
#include "openember/hardware/result.hpp"
#include "openember/sensor/sensor_info.hpp"

namespace openember::sensor {

class ISensor {
public:
    virtual ~ISensor() = default;

    virtual const SensorInfo& Info() const = 0;
    virtual hardware::DeviceStatus Status() const = 0;
    virtual hardware::Result<void> Start() = 0;
    virtual void Stop() noexcept = 0;
};

}  // namespace openember::sensor
