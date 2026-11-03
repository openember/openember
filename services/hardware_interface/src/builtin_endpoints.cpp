#include "openember/services/hardware_interface/endpoint_registry.hpp"
#include <stdexcept>
#if OPENEMBER_HARDWARE_ENDPOINT_IMU
#include "endpoints/imu_endpoint.hpp"
#endif
#if OPENEMBER_HARDWARE_ENDPOINT_TEMPERATURE
#include "endpoints/temperature_endpoint.hpp"
#endif
#if OPENEMBER_HARDWARE_ENDPOINT_GNSS
#include "endpoints/gnss_endpoint.hpp"
#endif
#if OPENEMBER_HARDWARE_ENDPOINT_JOINT_CONTROLLER
#include "endpoints/joint_controller_endpoint.hpp"
#endif

namespace openember::services::hardware_interface {
namespace {
template<class T> EndpointRegistry::Factory MockFactory() {
    return [](const EndpointConfig& config, const EndpointContext& context) {
        if (config.mode != "mock") throw std::invalid_argument("builtin endpoint supports mock mode only");
        return std::make_unique<T>(config, context);
    };
}
}
EndpointRegistry BuiltinEndpointRegistry() {
    EndpointRegistry registry;
#if OPENEMBER_HARDWARE_ENDPOINT_IMU
    registry.Register("imu", "mock_imu", {"openember.msgs.sensor.v1.ImuSample", "",
        openember::msgs::device::v1::DEVICE_CATEGORY_SENSOR}, MockFactory<ImuEndpoint>());
#endif
#if OPENEMBER_HARDWARE_ENDPOINT_TEMPERATURE
    registry.Register("temperature", "mock_temperature", {"openember.msgs.sensor.v1.TemperatureSample", "",
        openember::msgs::device::v1::DEVICE_CATEGORY_SENSOR}, MockFactory<TemperatureEndpoint>());
#endif
#if OPENEMBER_HARDWARE_ENDPOINT_GNSS
    registry.Register("gnss", "mock_gnss", {"openember.msgs.sensor.v1.GnssFix", "",
        openember::msgs::device::v1::DEVICE_CATEGORY_SENSOR}, MockFactory<GnssEndpoint>());
#endif
#if OPENEMBER_HARDWARE_ENDPOINT_JOINT_CONTROLLER
    registry.Register("joint_controller", "mock_joint_controller", {"openember.msgs.actuator.v1.JointState",
        "openember.msgs.actuator.v1.JointCommand", openember::msgs::device::v1::DEVICE_CATEGORY_ACTUATOR},
        MockFactory<JointControllerEndpoint>());
#endif
    return registry;
}
} // namespace openember::services::hardware_interface
