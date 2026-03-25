/* OpenEmber OSAL — C++ wrapper: Mutex (RAII) */
#ifndef OPENEMBER_OSAL_MUTEX_HPP_
#define OPENEMBER_OSAL_MUTEX_HPP_

#include "openember/osal/mutex.h"

namespace oe {
namespace osal {

class Mutex {
public:
    Mutex()
    {
        oe_mutex_init(&m_);
    }

    ~Mutex()
    {
        /* Safe even if not initialized; keep C-side semantics. */
        (void)oe_mutex_destroy(&m_);
    }

    Mutex(const Mutex &) = delete;
    Mutex &operator=(const Mutex &) = delete;

    Mutex(Mutex &&) = delete;
    Mutex &operator=(Mutex &&) = delete;

    oe_result_t lock()
    {
        return oe_mutex_lock(&m_);
    }

    oe_result_t try_lock()
    {
        return oe_mutex_trylock(&m_);
    }

    oe_result_t unlock()
    {
        return oe_mutex_unlock(&m_);
    }

    oe_mutex_t *native_handle()
    {
        return &m_;
    }

private:
    oe_mutex_t m_ {};
};

class MutexLock {
public:
    explicit MutexLock(Mutex &m) : m_(m)
    {
        r_ = m_.lock();
        locked_ = (r_ == OE_OK);
    }

    ~MutexLock()
    {
        if (locked_) {
            (void)m_.unlock();
        }
    }

    MutexLock(const MutexLock &) = delete;
    MutexLock &operator=(const MutexLock &) = delete;

    oe_result_t result() const
    {
        return r_;
    }

private:
    Mutex &m_;
    bool locked_ = false;
    oe_result_t r_ = OE_ERR_INTERNAL;
};

} // namespace osal
} // namespace oe

#endif /* OPENEMBER_OSAL_MUTEX_HPP_ */

