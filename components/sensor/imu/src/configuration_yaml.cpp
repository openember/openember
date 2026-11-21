#include "openember/sensor/imu_driver_yaml.hpp"
#include "openember/config/yaml.hpp"
namespace openember::sensor {
ImuDriverConfig LoadImuDriverConfig(const std::string& path, const std::string& sensor_id) {
    const auto root = config::LoadYamlFile(path);
    root.CheckKeys({"schema_version", "robot_id", "devices"});
    root.Require(root.Get<int>("schema_version") == 1, "expected schema_version: 1");
    root.Require(!root.Get<std::string>("robot_id").empty(), "robot_id must not be empty");
    const auto device = root.At("devices").At(sensor_id);
    device.CheckKeys({"type", "driver", "model", "enabled", "mode", "frame_id", "topic", "publish_rate_hz", "critical", "connection"});
    device.Require(device.Get<std::string>("type") == "imu", "selected device is not an IMU");
    device.Require(device.GetOr<bool>("enabled", true), "selected IMU is disabled");
    ImuDriverConfig c;
    c.sensor_id = sensor_id;
    c.driver = device.Get<std::string>("driver");
    c.frame_id = device.Get<std::string>("frame_id");
    c.model = device.GetOr<std::string>("model", "");
    if (c.driver == "yesense_readonly" || c.driver == "yesense") {
        device.Require(device.Get<std::string>("mode") == "readonly", "Yesense requires mode: readonly");
        device.Require(c.model == "YIS321", "unsupported Yesense model");
        c.driver = "yesense";
        const auto connection = device.At("connection");
        connection.CheckKeys({"device", "baud_rate", "rx_poll_wait_ms"});
        c.device = connection.Get<std::string>("device");
        c.baud_rate = connection.Get<std::uint32_t>("baud_rate");
        connection.Require(c.baud_rate != 0, "explicit nonzero baud_rate is required");
        c.rx_poll_wait_ms = connection.GetOr<std::uint32_t>("rx_poll_wait_ms", 5);
    } else if (c.driver == "mock" || c.driver == "mock_imu") {
        device.Require(device.GetOr<std::string>("mode", "mock") == "mock", "mock driver requires mode: mock");
        device.Require(!device.Find("connection"), "mock driver has no serial connection");
        c.driver = "mock";
        c.mock_rate_hz = device.GetOr<double>("publish_rate_hz", 100);
    }
    return c;
}
} // namespace openember::sensor
