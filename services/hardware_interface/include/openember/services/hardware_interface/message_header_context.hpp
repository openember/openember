#pragma once

#include <cstdint>
#include <string>

namespace openember::services::hardware_interface {

struct MessageHeaderContext {
    std::string source_node;
    std::string source_instance;
    std::string robot_id;
    std::uint64_t timestamp_unix_ns = 0;
};

}  // namespace openember::services::hardware_interface
