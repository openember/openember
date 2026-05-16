#define _POSIX_C_SOURCE 200809L

#include "openember/osal/time.hpp"

#include <ctime>
#include <errno.h>

namespace openember::osal {

Result sleep_ms(uint32_t ms)
{
    timespec req {};
    req.tv_sec = static_cast<time_t>(ms / 1000u);
    req.tv_nsec = static_cast<long>((ms % 1000u) * 1'000'000u);

    timespec rem {};
    while (nanosleep(&req, &rem) == -1) {
        if (errno != EINTR) {
            return kErrInternal;
        }
        req = rem;
    }
    return kOk;
}

Result sleep_ns(uint64_t ns)
{
    timespec req {};
    req.tv_sec = static_cast<time_t>(ns / 1'000'000'000ULL);
    req.tv_nsec = static_cast<long>(ns % 1'000'000'000ULL);

    timespec rem {};
    while (nanosleep(&req, &rem) == -1) {
        if (errno != EINTR) {
            return kErrInternal;
        }
        req = rem;
    }
    return kOk;
}

Result clock_monotonic_ns(uint64_t* out_ns)
{
    if (!out_ns) {
        return kErrInvalidArg;
    }

    timespec ts {};
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
        return kErrInternal;
    }

    *out_ns = static_cast<uint64_t>(ts.tv_sec) * 1'000'000'000ULL +
              static_cast<uint64_t>(ts.tv_nsec);
    return kOk;
}

}  // namespace openember::osal

extern "C" {

#include "openember/osal/time.h"

oe_result_t oe_sleep_ms(uint32_t ms)
{
    return static_cast<oe_result_t>(openember::osal::sleep_ms(ms));
}

oe_result_t oe_sleep_ns(uint64_t ns)
{
    return static_cast<oe_result_t>(openember::osal::sleep_ns(ns));
}

oe_result_t oe_clock_monotonic_ns(uint64_t* out_ns)
{
    return static_cast<oe_result_t>(openember::osal::clock_monotonic_ns(out_ns));
}

}  // extern "C"
