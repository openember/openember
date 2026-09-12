#include "openember/services/hardware_interface/endpoint_status.hpp"

namespace openember::services::hardware_interface {

const char* ToString(EndpointState state) noexcept {
    switch (state) {
    case EndpointState::kCreated:
        return "created";
    case EndpointState::kConfigured:
        return "configured";
    case EndpointState::kRunning:
        return "running";
    case EndpointState::kStopping:
        return "stopping";
    case EndpointState::kStopped:
        return "stopped";
    case EndpointState::kError:
        return "error";
    }
    return "unknown";
}

}  // namespace openember::services::hardware_interface
