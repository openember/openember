#pragma once

#include "adapters/message_header_context.hpp"
#include "openember/msgs/sensor/v1/sensor.pb.h"
#include "openember/sensor/gnss.hpp"
#include "openember/sensor/imu.hpp"
#include "openember/sensor/temperature.hpp"

namespace openember::services::hardware_interface {

openember::msgs::sensor::v1::ImuSample ToProto(
    const openember::sensor::ImuSample& sample,
    const MessageHeaderContext& context);

openember::msgs::sensor::v1::TemperatureSample ToProto(
    const openember::sensor::TemperatureSample& sample,
    const MessageHeaderContext& context);

openember::msgs::sensor::v1::GnssFix ToProto(
    const openember::sensor::GnssFix& fix,
    const MessageHeaderContext& context);

}  // namespace openember::services::hardware_interface
