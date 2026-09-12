#pragma once

#include <chrono>
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "openember/msgs/device/v1/device.pb.h"
#include "openember/msgs/diagnostics/v1/diagnostics.pb.h"
#include "openember/msgs/node/v1/node.pb.h"
#include "openember/node.hpp"
#include "openember/publisher.hpp"
#include "openember/services/hardware_interface/endpoint.hpp"

namespace openember::services::hardware_interface {

struct HardwareInterfaceAppOptions {
    std::string robot_id = "openember";
    std::string node_name = "hardware_interface";
    std::string instance_id = "hardware_interface";
};

class HardwareInterfaceApp {
public:
    HardwareInterfaceApp(std::shared_ptr<openember::Node> node,
                         HardwareInterfaceConfig config,
                         HardwareInterfaceAppOptions options = {});
    ~HardwareInterfaceApp();

    bool Start();
    void Stop() noexcept;
    void Spin();

private:
    bool BuildEndpoints();
    const EndpointConfig* FindEndpointConfig(const std::string& endpoint_id) const;
    openember::msgs::node::v1::NodeInfo BuildNodeInfo(std::uint64_t sequence) const;
    openember::msgs::node::v1::NodeHeartbeat BuildHeartbeat(std::uint64_t sequence) const;
    openember::msgs::diagnostics::v1::DiagnosticArray BuildDiagnostics(
        std::uint64_t sequence) const;
    openember::msgs::device::v1::DeviceInfo BuildDeviceInfo(
        const EndpointConfig& config,
        const EndpointStatus& status,
        std::uint64_t sequence) const;
    openember::msgs::device::v1::DeviceState BuildDeviceState(
        const EndpointConfig& config,
        const EndpointStatus& status,
        std::uint64_t sequence) const;

    std::shared_ptr<openember::Node> node_;
    HardwareInterfaceConfig config_;
    HardwareInterfaceAppOptions options_;
    std::vector<std::unique_ptr<HardwareEndpoint>> endpoints_;
    openember::Publisher<openember::msgs::node::v1::NodeInfo> node_info_pub_;
    openember::Publisher<openember::msgs::node::v1::NodeHeartbeat> heartbeat_pub_;
    openember::Publisher<openember::msgs::diagnostics::v1::DiagnosticArray> diagnostics_pub_;
    std::map<std::string, openember::Publisher<openember::msgs::device::v1::DeviceInfo>>
        device_info_pubs_;
    std::map<std::string, openember::Publisher<openember::msgs::device::v1::DeviceState>>
        device_state_pubs_;
    std::chrono::steady_clock::time_point start_time_;
    std::uint64_t start_time_unix_ns_ = 0;
};

}  // namespace openember::services::hardware_interface
