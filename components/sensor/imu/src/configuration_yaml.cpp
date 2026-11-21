#include "openember/sensor/imu_driver_yaml.hpp"
#include "openember/config/yaml.hpp"
#include <algorithm>
namespace openember::sensor {
ImuDriverConfig LoadImuDriverConfig(const std::string& path, const std::string& sensor_id) {
    const auto root = config::LoadYamlFile(path);
    root.CheckKeys({"schema_version", "robot_id", "devices"});
    root.Require(root.Get<int>("schema_version") == 1, "expected schema_version: 1");
    root.Require(!root.Get<std::string>("robot_id").empty(), "robot_id must not be empty");
    const auto device = root.At("devices").At(sensor_id);
    device.CheckKeys({"type", "driver", "model", "enabled", "mode", "frame_id", "topic", "publish_rate_hz", "critical", "connection", "orientation", "mounting"});
    device.Require(device.Get<std::string>("type") == "imu", "selected device is not an IMU");
    device.Require(device.GetOr<bool>("enabled", true), "selected IMU is disabled");
    ImuDriverConfig c;
    c.sensor_id = sensor_id;
    c.driver = device.Get<std::string>("driver");
    c.frame_id = device.Get<std::string>("frame_id");
    c.model = device.GetOr<std::string>("model", "");
    if (device.Find("orientation") || device.Find("mounting")) {
        device.Require(c.driver=="yesense_readonly" || c.driver=="yesense", "installation requires supported YESENSE profile");
        const auto orientation=device.At("orientation"),mount=device.At("mounting");
        orientation.CheckKeys({"convention","reference_frame_id"}); mount.CheckKeys({"body_frame_id","rotation_sensor_to_body_wxyz","revision"});
        orientation.Require(orientation.Get<std::string>("convention")=="sensor_to_reference_z_up","unsupported IMU convention");
        auto& m=c.installation; m.enabled=true; m.sensor_frame=c.frame_id; m.reference_frame=orientation.Get<std::string>("reference_frame_id");
        m.body_frame=mount.Get<std::string>("body_frame_id"); m.revision=mount.Get<std::string>("revision");
        const auto q=mount.Get<std::vector<double>>("rotation_sensor_to_body_wxyz"); mount.Require(q.size()==4,"expected four wxyz values");
        std::copy(q.begin(),q.end(),m.sensor_to_body.begin()); ValidateImuInstallation(m);
    }
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
