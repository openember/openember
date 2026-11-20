#pragma once
#include <cstdint>

namespace openember::sensor::detail {

// YIS321 wire TID is 1..60000. previous==0 denotes no accepted frame yet.
// Half-cycle ambiguity is rejected; this cannot detect all device resets/replay.
inline bool IsNewYesenseTid(std::uint16_t current, std::uint16_t previous) {
    constexpr std::uint32_t cycle = 60000;
    if (current == 0 || current > cycle || previous > cycle) return false;
    if (previous == 0) return true;
    const auto advance = (static_cast<std::uint32_t>(current) + cycle - previous) % cycle;
    return advance > 0 && advance < cycle / 2;
}

} // namespace openember::sensor::detail
