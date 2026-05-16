#pragma once

#include <ctime>

#include "openember/osal/types.hpp"

namespace openember::osal::detail {

inline Result monotonic_deadline_from_timeout_ms(int timeout_ms, timespec* out_ts)
{
    if (!out_ts) {
        return kErrInvalidArg;
    }

    timespec now {};
    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) {
        return kErrInternal;
    }

    const long nsec = now.tv_nsec + static_cast<long>(timeout_ms % 1000) * 1'000'000L;
    out_ts->tv_sec = now.tv_sec + timeout_ms / 1000 + nsec / 1'000'000'000L;
    out_ts->tv_nsec = nsec % 1'000'000'000L;
    return kOk;
}

inline Result realtime_deadline_from_timeout_ms(int timeout_ms, timespec* out_ts)
{
    if (!out_ts) {
        return kErrInvalidArg;
    }

    timespec now {};
    if (clock_gettime(CLOCK_REALTIME, &now) != 0) {
        return kErrInternal;
    }

    const long nsec = now.tv_nsec + static_cast<long>(timeout_ms % 1000) * 1'000'000L;
    out_ts->tv_sec = now.tv_sec + timeout_ms / 1000 + nsec / 1'000'000'000L;
    out_ts->tv_nsec = nsec % 1'000'000'000L;
    return kOk;
}

}  // namespace openember::osal::detail
