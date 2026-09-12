#pragma once

#include <memory>
#include <string>

#include "openember/node.hpp"

namespace openember::services::hardware_interface {

struct EndpointContext {
    std::shared_ptr<openember::Node> node;
    std::string source_node;
    std::string source_instance;
    std::string robot_id;
};

}  // namespace openember::services::hardware_interface
