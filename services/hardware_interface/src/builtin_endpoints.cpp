#include "openember/services/hardware_interface/endpoint_registry.hpp"
#include <stdexcept>
#if OPENEMBER_HI_YESENSE
#include "openember/sensor/yesense_imu.hpp"
#include "openember/services/hardware_interface/imu_endpoint_factory.hpp"
#endif
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
#if OPENEMBER_HI_YESENSE
    registry.Register("imu", "yesense_readonly", {"openember.msgs.sensor.v1.ImuSample", "",
        openember::msgs::device::v1::DEVICE_CATEGORY_SENSOR},
        [](const EndpointConfig& config, const EndpointContext& context) {
            if (config.mode != "readonly") throw std::invalid_argument("YESENSE requires readonly mode");
            openember::sensor::YesenseImuOptions options;
            options.device = config.connection.device;
            options.baud_rate = config.connection.baud_rate;
            options.rx_poll_wait_ms = config.connection.rx_poll_wait_ms;
            options.sensor_id = config.endpoint_id;
            options.frame_id = config.frame_id;
            options.model = config.model;
            return MakeImuEndpoint(config, context, std::make_unique<openember::sensor::YesenseImu>(std::move(options)));
        });
#endif
    return registry;
}
} // namespace openember::services::hardware_interface
