/* OpenEmber OSAL — C++ wrapper: Signals (signalfd) */
#ifndef OPENEMBER_OSAL_SIGNALS_HPP_
#define OPENEMBER_OSAL_SIGNALS_HPP_

#include "openember/osal/signals.h"

#include <cstddef>

namespace oe {
namespace osal {

class Signals {
public:
    Signals() = default;
    Signals(const Signals &) = delete;
    Signals &operator=(const Signals &) = delete;

    ~Signals()
    {
        (void)oe_signals_close(&s_);
    }

    oe_result_t open(const int *signums, size_t count)
    {
        return oe_signals_open(&s_, signums, count);
    }

    oe_result_t wait(oe_signal_info_t *out_info, int timeout_ms)
    {
        return oe_signals_wait(&s_, out_info, timeout_ms);
    }

    oe_result_t close()
    {
        return oe_signals_close(&s_);
    }

    oe_result_t query_caps(oe_signals_caps_t *out_caps) const
    {
        return oe_signals_query_caps(out_caps);
    }

private:
    oe_signals_t s_ {};
};

} // namespace osal
} // namespace oe

#endif /* OPENEMBER_OSAL_SIGNALS_HPP_ */

