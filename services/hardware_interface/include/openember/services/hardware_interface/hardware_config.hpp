#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "openember/hardware/result.hpp"

namespace openember::services::hardware_interface {

struct EndpointConfig {
    std::string endpoint_id;
    std::string type;
    bool enabled = true;
    std::string mode = "mock";
    std::string driver;
    std::string topic;
    std::string command_topic;
    std::string state_topic;
    std::string frame_id;
    double publish_rate_hz = 1.0;
    std::uint64_t command_timeout_ms = 100;
    bool disabled_on_start = true;
    bool critical = false;
};

struct HardwareInterfaceConfig {
    std::string robot_id = "openember";
    std::string node_name = "hardware_interface";
    std::string instance_id = "hardware_interface";
    std::vector<EndpointConfig> endpoints;
};

HardwareInterfaceConfig DefaultMockConfig();
openember::hardware::Result<HardwareInterfaceConfig> LoadConfigFromFile(
    const std::string& path);

}  // namespace openember::services::hardware_interface
