/* OpenEmber OSAL — C++ wrapper: Thread */
#ifndef OPENEMBER_OSAL_THREAD_HPP_
#define OPENEMBER_OSAL_THREAD_HPP_

#include "openember/osal/thread.h"

#include <functional>
#include <utility>

namespace oe {
namespace osal {

class Thread {
public:
    Thread() = default;

    Thread(const Thread &) = delete;
    Thread &operator=(const Thread &) = delete;

    Thread(Thread &&) = delete;
    Thread &operator=(Thread &&) = delete;

    template <typename F>
    oe_result_t start(F &&fn)
    {
        using FnT = std::function<void()>;
        FnT *heap_fn = new FnT(std::forward<F>(fn));
        oe_result_t r = oe_thread_create(&t_, &Thread::trampoline, heap_fn);
        started_ = (r == OE_OK);
        if (r != OE_OK) {
            delete heap_fn;
        }
        return r;
    }

    oe_result_t join()
    {
        started_ = false;
        return oe_thread_join(&t_);
    }

private:
    static void trampoline(void *arg)
    {
        std::function<void()> *fn = (std::function<void()> *)arg;
        if (fn) {
            (*fn)();
            delete fn;
        }
    }

private:
    oe_thread_t t_ {};
    bool started_ = false;
};

} // namespace osal
} // namespace oe

#endif /* OPENEMBER_OSAL_THREAD_HPP_ */

