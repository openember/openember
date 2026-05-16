#pragma once

#include <cstdint>

namespace openember::osal {

using Result = int;

inline constexpr Result kOk = 0;
inline constexpr Result kErrInvalidArg = -1;
inline constexpr Result kErrNoMem = -2;
inline constexpr Result kErrBusy = -3;
inline constexpr Result kErrAgain = -4;
inline constexpr Result kErrInternal = -5;
inline constexpr Result kErrUnsupported = -6;
inline constexpr Result kErrDeadlock = -7;
inline constexpr Result kErrTimeout = -8;

}  // namespace openember::osal
