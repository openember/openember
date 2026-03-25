/* OpenEmber OSAL — C++ wrapper: Semaphore (RAII) */
#ifndef OPENEMBER_OSAL_SEM_HPP_
#define OPENEMBER_OSAL_SEM_HPP_

#include "openember/osal/sem.h"

namespace oe {
namespace osal {

class Semaphore {
public:
    Semaphore() = default;

    explicit Semaphore(uint32_t initial_count)
    {
        (void)init(initial_count);
    }

    ~Semaphore()
    {
        if (inited_) {
            (void)oe_sem_destroy(&s_);
        }
    }

    Semaphore(const Semaphore &) = delete;
    Semaphore &operator=(const Semaphore &) = delete;

    oe_result_t init(uint32_t initial_count)
    {
        inited_ = false;
        oe_result_t r = oe_sem_init(&s_, initial_count);
        if (r == OE_OK) {
            inited_ = true;
        }
        return r;
    }

    oe_result_t destroy()
    {
        oe_result_t r = OE_OK;
        if (inited_) {
            r = oe_sem_destroy(&s_);
            inited_ = false;
        }
        return r;
    }

    oe_result_t post()
    {
        if (!inited_) {
            return OE_ERR_INVALID_ARG;
        }
        return oe_sem_post(&s_);
    }

    oe_result_t wait()
    {
        if (!inited_) {
            return OE_ERR_INVALID_ARG;
        }
        return oe_sem_wait(&s_);
    }

    oe_result_t trywait()
    {
        if (!inited_) {
            return OE_ERR_INVALID_ARG;
        }
        return oe_sem_trywait(&s_);
    }

    oe_result_t wait_timeout_ms(int timeout_ms)
    {
        if (!inited_) {
            return OE_ERR_INVALID_ARG;
        }
        return oe_sem_wait_timeout_ms(&s_, timeout_ms);
    }

private:
    oe_sem_t s_ {};
    bool inited_ = false;
};

} // namespace osal
} // namespace oe

#endif /* OPENEMBER_OSAL_SEM_HPP_ */

