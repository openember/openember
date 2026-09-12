#pragma once

#include <string>

#include "openember/services/hardware_interface/endpoint_context.hpp"
#include "openember/services/hardware_interface/endpoint_status.hpp"
#include "openember/services/hardware_interface/hardware_config.hpp"

namespace openember::services::hardware_interface {

class HardwareEndpoint {
public:
    virtual ~HardwareEndpoint() = default;

    virtual const std::string& Id() const = 0;
    virtual bool Configure() = 0;
    virtual bool Start() = 0;
    virtual void Stop() noexcept = 0;
    virtual EndpointStatus Status() const = 0;
};

}  // namespace openember::services::hardware_interface
