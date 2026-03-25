#define _POSIX_C_SOURCE 200809L

#include "openember/osal/time.h"

#include <errno.h>
#include <time.h>

oe_result_t oe_sleep_ms(uint32_t ms)
{
    struct timespec req;
    struct timespec rem;

    req.tv_sec = (time_t)(ms / 1000u);
    req.tv_nsec = (long)((ms % 1000u) * 1000000u);

    while (nanosleep(&req, &rem) == -1) {
        if (errno != EINTR) {
            return OE_ERR_INTERNAL;
        }
        req = rem;
    }
    return OE_OK;
}

oe_result_t oe_sleep_ns(uint64_t ns)
{
    struct timespec req;
    struct timespec rem;

    req.tv_sec = (time_t)(ns / 1000000000ULL);
    req.tv_nsec = (long)(ns % 1000000000ULL);

    while (nanosleep(&req, &rem) == -1) {
        if (errno != EINTR) {
            return OE_ERR_INTERNAL;
        }
        req = rem;
    }
    return OE_OK;
}

oe_result_t oe_clock_monotonic_ns(uint64_t *out_ns)
{
    struct timespec ts;

    if (!out_ns) {
        return OE_ERR_INVALID_ARG;
    }
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
        return OE_ERR_INTERNAL;
    }
    *out_ns = (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
    return OE_OK;
}
