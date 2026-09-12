#include "openember/services/hardware_interface/hardware_config.hpp"

#include <cstdint>
#include <stdexcept>
#include <unordered_set>
#include <utility>

#include <yaml-cpp/yaml.h>

namespace openember::services::hardware_interface {

namespace {

openember::hardware::Result<HardwareInterfaceConfig> ConfigError(
    const std::string& message) {
    return openember::hardware::Result<HardwareInterfaceConfig>::Failure(
        openember::hardware::MakeError(openember::hardware::ErrorCode::kInvalidConfig,
                                       message));
}

std::string EndpointPrefix(std::size_t index) {
    return "endpoints[" + std::to_string(index) + "]";
}

std::string RequiredString(const YAML::Node& node,
                           const char* key,
                           const std::string& prefix) {
    const auto value = node[key];
    if (!value || !value.IsScalar()) {
        throw std::runtime_error(prefix + "." + key + " is required");
    }
    auto text = value.as<std::string>();
    if (text.empty()) {
        throw std::runtime_error(prefix + "." + key + " must not be empty");
    }
    return text;
}

std::string OptionalString(const YAML::Node& node,
                           const char* key,
                           std::string fallback,
                           const std::string& prefix) {
    const auto value = node[key];
    if (!value) {
        return fallback;
    }
    if (!value.IsScalar()) {
        throw std::runtime_error(prefix + "." + key + " must be a scalar");
    }
    return value.as<std::string>();
}

bool OptionalBool(const YAML::Node& node,
                  const char* key,
                  bool fallback,
                  const std::string& prefix) {
    const auto value = node[key];
    if (!value) {
        return fallback;
    }
    if (!value.IsScalar()) {
        throw std::runtime_error(prefix + "." + key + " must be a boolean");
    }
    return value.as<bool>();
}

double OptionalDouble(const YAML::Node& node,
                      const char* key,
                      double fallback,
                      const std::string& prefix) {
    const auto value = node[key];
    if (!value) {
        return fallback;
    }
    if (!value.IsScalar()) {
        throw std::runtime_error(prefix + "." + key + " must be a number");
    }
    return value.as<double>();
}

std::uint64_t OptionalUint64(const YAML::Node& node,
                             const char* key,
                             std::uint64_t fallback,
                             const std::string& prefix) {
    const auto value = node[key];
    if (!value) {
        return fallback;
    }
    if (!value.IsScalar()) {
        throw std::runtime_error(prefix + "." + key + " must be an unsigned integer");
    }
    return value.as<std::uint64_t>();
}

std::string DefaultDriverFor(const std::string& type, const std::string& mode) {
    if (mode == "mock") {
        if (type == "imu") {
            return "mock_imu";
        }
        if (type == "temperature") {
            return "mock_temperature";
        }
        if (type == "gnss") {
            return "mock_gnss";
        }
        if (type == "joint_controller") {
            return "mock_joint_controller";
        }
        return "mock_" + type;
    }
    return type;
}

double DefaultRateFor(const std::string& type) {
    if (type == "imu") {
        return 100.0;
    }
    if (type == "temperature") {
        return 2.0;
    }
    if (type == "gnss") {
        return 1.0;
    }
    if (type == "joint_controller") {
        return 100.0;
    }
    return 1.0;
}

std::string DefaultTopicFor(const std::string& type, const std::string& endpoint_id) {
    if (type == "imu") {
        return "/sensors/imu/" + endpoint_id + "/sample";
    }
    if (type == "temperature") {
        return "/sensors/temperature/" + endpoint_id + "/sample";
    }
    if (type == "gnss") {
        return "/sensors/gnss/" + endpoint_id + "/fix";
    }
    if (type == "joint_controller") {
        return "/actuators/joints/" + endpoint_id + "/state";
    }
    return "/hardware/" + type + "/" + endpoint_id;
}

std::string DefaultCommandTopicFor(const std::string& type,
                                   const std::string& endpoint_id) {
    if (type == "joint_controller") {
        return "/actuators/joints/" + endpoint_id + "/command";
    }
    return "";
}

EndpointConfig ParseEndpoint(const YAML::Node& node, std::size_t index) {
    const auto prefix = EndpointPrefix(index);
    if (!node || !node.IsMap()) {
        throw std::runtime_error(prefix + " must be a map");
    }

    EndpointConfig config;
    config.endpoint_id = OptionalString(node, "id", "", prefix);
    config.endpoint_id =
        OptionalString(node, "endpoint_id", config.endpoint_id, prefix);
    if (config.endpoint_id.empty()) {
        throw std::runtime_error(prefix + ".id is required");
    }

    config.type = RequiredString(node, "type", prefix);
    config.enabled = OptionalBool(node, "enabled", true, prefix);
    config.mode = OptionalString(node, "mode", "mock", prefix);
    config.driver = OptionalString(
        node, "driver", DefaultDriverFor(config.type, config.mode), prefix);
    config.topic = OptionalString(
        node, "topic", DefaultTopicFor(config.type, config.endpoint_id), prefix);
    config.state_topic = OptionalString(node, "state_topic", config.topic, prefix);
    config.command_topic = OptionalString(
        node,
        "command_topic",
        DefaultCommandTopicFor(config.type, config.endpoint_id),
        prefix);
    config.frame_id =
        OptionalString(node, "frame_id", config.endpoint_id + "_link", prefix);
    config.publish_rate_hz =
        OptionalDouble(node, "publish_rate_hz", DefaultRateFor(config.type), prefix);
    config.command_timeout_ms =
        OptionalUint64(node, "command_timeout_ms", 100, prefix);
    config.disabled_on_start =
        OptionalBool(node, "disabled_on_start", true, prefix);
    config.critical = OptionalBool(node, "critical", false, prefix);

    if (config.mode.empty()) {
        throw std::runtime_error(prefix + ".mode must not be empty");
    }
    if (config.driver.empty()) {
        throw std::runtime_error(prefix + ".driver must not be empty");
    }
    if (config.topic.empty()) {
        throw std::runtime_error(prefix + ".topic must not be empty");
    }
    if (config.type == "joint_controller") {
        if (config.state_topic.empty()) {
            throw std::runtime_error(prefix + ".state_topic must not be empty");
        }
        if (config.command_topic.empty()) {
            throw std::runtime_error(prefix + ".command_topic must not be empty");
        }
        config.topic = config.state_topic;
    }
    if (config.publish_rate_hz <= 0.0) {
        throw std::runtime_error(prefix + ".publish_rate_hz must be > 0");
    }

    return config;
}

}  // namespace

HardwareInterfaceConfig DefaultMockConfig() {
    HardwareInterfaceConfig config;

#if OPENEMBER_HARDWARE_ENDPOINT_IMU
    config.endpoints.push_back(EndpointConfig{
        "imu0",
        "imu",
        true,
        "mock",
        "mock_imu",
        "/sensors/imu/imu0/sample",
        "",
        "/sensors/imu/imu0/sample",
        "imu_link",
        100.0,
        100,
        true,
        false});
#endif

#if OPENEMBER_HARDWARE_ENDPOINT_TEMPERATURE
    config.endpoints.push_back(EndpointConfig{
        "temp0",
        "temperature",
        true,
        "mock",
        "mock_temperature",
        "/sensors/temperature/temp0/sample",
        "",
        "/sensors/temperature/temp0/sample",
        "temperature_link",
        2.0,
        100,
        true,
        false});
#endif

#if OPENEMBER_HARDWARE_ENDPOINT_GNSS
    config.endpoints.push_back(EndpointConfig{
        "gnss0",
        "gnss",
        true,
        "mock",
        "mock_gnss",
        "/sensors/gnss/gnss0/fix",
        "",
        "/sensors/gnss/gnss0/fix",
        "gnss_link",
        1.0,
        100,
        true,
        false});
#endif

#if OPENEMBER_HARDWARE_ENDPOINT_JOINT_CONTROLLER
    config.endpoints.push_back(EndpointConfig{
        "joint_controller0",
        "joint_controller",
        true,
        "mock",
        "mock_joint_controller",
        "/actuators/joints/joint_controller0/state",
        "/actuators/joints/joint_controller0/command",
        "/actuators/joints/joint_controller0/state",
        "base_link",
        100.0,
        100,
        true,
        false});
#endif

    return config;
}

openember::hardware::Result<HardwareInterfaceConfig> LoadConfigFromFile(
    const std::string& path) {
    if (path.empty()) {
        return ConfigError("config path must not be empty");
    }

    try {
        const auto root = YAML::LoadFile(path);
        if (!root || !root.IsMap()) {
            return ConfigError("hardware_interface config root must be a map");
        }

        HardwareInterfaceConfig config;
        const auto app_node = root["hardware_interface"];
        if (app_node) {
            if (!app_node.IsMap()) {
                return ConfigError("hardware_interface must be a map");
            }
            config.robot_id =
                OptionalString(app_node, "robot_id", config.robot_id, "hardware_interface");
            config.node_name =
                OptionalString(app_node, "node_name", config.node_name, "hardware_interface");
            config.instance_id = OptionalString(
                app_node, "instance_id", config.instance_id, "hardware_interface");
        }

        auto endpoints = root["endpoints"];
        if (!endpoints && app_node) {
            endpoints = app_node["endpoints"];
        }
        if (!endpoints || !endpoints.IsSequence()) {
            return ConfigError("endpoints must be a sequence");
        }

        std::unordered_set<std::string> endpoint_ids;
        for (std::size_t i = 0; i < endpoints.size(); ++i) {
            auto endpoint = ParseEndpoint(endpoints[i], i);
            if (!endpoint_ids.insert(endpoint.endpoint_id).second) {
                return ConfigError("duplicate endpoint id: " + endpoint.endpoint_id);
            }
            config.endpoints.push_back(std::move(endpoint));
        }

        return openember::hardware::Result<HardwareInterfaceConfig>::Success(
            std::move(config));
    } catch (const YAML::Exception& e) {
        return ConfigError(std::string("failed to parse hardware_interface config: ") +
                           e.what());
    } catch (const std::exception& e) {
        return ConfigError(e.what());
    }
}

}  // namespace openember::services::hardware_interface
