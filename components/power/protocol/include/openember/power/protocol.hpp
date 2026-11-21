#pragma once
#include "openember/power/controller.hpp"
#include "openember/msgs/power/v1/power.pb.h"

namespace openember::power {
// Call only on the device worker. Authentication and readiness are external;
// neither wire header nor request contents grant authority.
openember::msgs::power::v1::PowerControlResult HandleControl(
    Controller&, const openember::msgs::power::v1::PowerControlRequest&,
    const Context&, std::uint64_t now_monotonic_ns);
openember::msgs::power::v1::PowerCapabilities Describe(const Controller&);
} // namespace openember::power
