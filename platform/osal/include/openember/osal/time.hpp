/* OpenEmber OSAL — C++ wrapper: Time */
#ifndef OPENEMBER_OSAL_TIME_HPP_
#define OPENEMBER_OSAL_TIME_HPP_

#include "openember/osal/time.h"

namespace oe {
namespace osal {

inline oe_result_t sleep_ms(unsigned int ms)
{
    return oe_sleep_ms((uint32_t)ms);
}

inline oe_result_t sleep_ns(unsigned long long ns)
{
    return oe_sleep_ns((uint64_t)ns);
}

inline oe_result_t clock_monotonic_ns(uint64_t *out_ns)
{
    return oe_clock_monotonic_ns(out_ns);
}

} // namespace osal
} // namespace oe

#endif /* OPENEMBER_OSAL_TIME_HPP_ */

