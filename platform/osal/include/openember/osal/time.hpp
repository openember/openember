#pragma once

#include <cstdint>

#include "openember/osal/types.hpp"

namespace openember::osal {

Result sleep_ms(uint32_t ms);
Result sleep_ns(uint64_t ns);
Result clock_monotonic_ns(uint64_t* out_ns);

}  // namespace openember::osal
