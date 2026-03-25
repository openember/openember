/* OpenEmber OSAL — C++ wrapper: Cond (RAII) */
#ifndef OPENEMBER_OSAL_COND_HPP_
#define OPENEMBER_OSAL_COND_HPP_

#include "openember/osal/cond.h"

#include "openember/osal/mutex.h"

namespace oe {
namespace osal {

class Cond {
public:
    Cond()
    {
        inited_ = (oe_cond_init(&c_) == OE_OK);
    }

    ~Cond()
    {
        if (inited_) {
            (void)oe_cond_destroy(&c_);
        }
    }

    Cond(const Cond &) = delete;
    Cond &operator=(const Cond &) = delete;

    oe_result_t wait(Mutex &m)
    {
        return oe_cond_wait(&c_, m.native_handle());
    }

    oe_result_t wait_timeout_ms(Mutex &m, int timeout_ms)
    {
        return oe_cond_wait_timeout_ms(&c_, m.native_handle(), timeout_ms);
    }

private:
    oe_cond_t c_ {};
    bool inited_ = false;
};

} // namespace osal
} // namespace oe

#endif /* OPENEMBER_OSAL_COND_HPP_ */

