/* OpenEmber OSAL — C++ wrapper: Event (RAII, auto-reset) */
#ifndef OPENEMBER_OSAL_EVENT_HPP_
#define OPENEMBER_OSAL_EVENT_HPP_

#include "openember/osal/event.h"

namespace oe {
namespace osal {

class Event {
public:
    Event()
    {
        inited_ = (oe_event_init(&e_) == OE_OK);
    }

    ~Event()
    {
        if (inited_) {
            (void)oe_event_destroy(&e_);
        }
    }

    Event(const Event &) = delete;
    Event &operator=(const Event &) = delete;

    oe_result_t set()
    {
        return oe_event_set(&e_);
    }

    oe_result_t reset()
    {
        return oe_event_reset(&e_);
    }

    oe_result_t wait()
    {
        return oe_event_wait(&e_);
    }

    oe_result_t wait_timeout_ms(int timeout_ms)
    {
        return oe_event_wait_timeout_ms(&e_, timeout_ms);
    }

private:
    oe_event_t e_ {};
    bool inited_ = false;
};

} // namespace osal
} // namespace oe

#endif /* OPENEMBER_OSAL_EVENT_HPP_ */

