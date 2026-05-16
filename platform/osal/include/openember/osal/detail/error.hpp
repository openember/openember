#pragma once

#include <cerrno>

#include "openember/osal/types.hpp"

namespace openember::osal::detail {

inline Result from_errno(int err) noexcept
{
    switch (err) {
        case EAGAIN:
            return kErrAgain;
        case EBUSY:
            return kErrBusy;
        case ETIMEDOUT:
            return kErrTimeout;
        case EDEADLK:
            return kErrDeadlock;
        case ENOMEM:
            return kErrNoMem;
        default:
            return kErrInternal;
    }
}

}  // namespace openember::osal::detail
