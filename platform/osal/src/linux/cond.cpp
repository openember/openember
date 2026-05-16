#define _POSIX_C_SOURCE 200809L

#include "openember/osal/cond.hpp"

#include <pthread.h>

#include "openember/osal/detail/deadline.hpp"
#include "openember/osal/detail/error.hpp"

namespace openember::osal {

struct Cond::Impl {
    pthread_cond_t native {};
    bool inited = false;
};

Cond::Cond() : impl_(std::make_unique<Impl>())
{
    if (pthread_cond_init(&impl_->native, nullptr) == 0) {
        impl_->inited = true;
    }
}

Cond::~Cond()
{
    if (impl_ && impl_->inited) {
        pthread_cond_destroy(&impl_->native);
    }
}

Result Cond::wait(Mutex& mutex)
{
    pthread_mutex_t* m = mutex.native_handle();
    if (!impl_ || !impl_->inited || !m) {
        return kErrInvalidArg;
    }

    const int rc = pthread_cond_wait(&impl_->native, m);
    return rc == 0 ? kOk : detail::from_errno(rc);
}

Result Cond::wait_timeout_ms(Mutex& mutex, int timeout_ms)
{
    pthread_mutex_t* m = mutex.native_handle();
    if (!impl_ || !impl_->inited || !m) {
        return kErrInvalidArg;
    }

    timespec abs_ts {};
    const Result deadline = detail::monotonic_deadline_from_timeout_ms(timeout_ms, &abs_ts);
    if (deadline != kOk) {
        return deadline;
    }

    const int rc = pthread_cond_timedwait(&impl_->native, m, &abs_ts);
    if (rc == 0) {
        return kOk;
    }
    if (rc == ETIMEDOUT) {
        return kErrTimeout;
    }
    return detail::from_errno(rc);
}

Result Cond::signal()
{
    if (!impl_ || !impl_->inited) {
        return kErrInvalidArg;
    }
    return pthread_cond_signal(&impl_->native) == 0 ? kOk : kErrInternal;
}

Result Cond::broadcast()
{
    if (!impl_ || !impl_->inited) {
        return kErrInvalidArg;
    }
    return pthread_cond_broadcast(&impl_->native) == 0 ? kOk : kErrInternal;
}

}  // namespace openember::osal
