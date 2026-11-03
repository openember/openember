#pragma once

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include "openember/msgs/device/v1/device.pb.h"
#include "openember/services/hardware_interface/endpoint.hpp"

namespace openember::services::hardware_interface {

struct EndpointDescriptor {
    std::string sample_type;
    std::string command_type;
    openember::msgs::device::v1::DeviceCategory category =
        openember::msgs::device::v1::DEVICE_CATEGORY_UNSPECIFIED;
};

// Explicit, process-local composition. Register before constructing the app;
// no static registration, dlopen, wildcard fallback, or silent replacement.
class EndpointRegistry {
public:
    using Factory = std::function<std::unique_ptr<HardwareEndpoint>(
        const EndpointConfig&, const EndpointContext&)>;
    void Register(std::string type, std::string driver, EndpointDescriptor descriptor, Factory factory);
    std::unique_ptr<HardwareEndpoint> Create(const EndpointConfig& config,
                                              const EndpointContext& context) const;
    const EndpointDescriptor* Describe(const EndpointConfig& config) const;
private:
    struct Entry { EndpointDescriptor descriptor; Factory factory; };
    std::map<std::pair<std::string, std::string>, Entry> entries_;
};

EndpointRegistry BuiltinEndpointRegistry();
} // namespace openember::services::hardware_interface
