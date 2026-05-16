#define _POSIX_C_SOURCE 200809L

#include "openember/osal/event.hpp"

#include <cerrno>
#include <pthread.h>
#include <time.h>

#include <cstring>

#include "openember/osal/detail/deadline.hpp"

namespace openember::osal {

struct Event::Impl {
    pthread_mutex_t mutex {};
    pthread_cond_t cond {};
    int signaled = 0;
    bool inited = false;
};

Event::Event() : impl_(std::make_unique<Impl>())
{
    if (pthread_mutex_init(&impl_->mutex, nullptr) != 0) {
        return;
    }
    if (pthread_cond_init(&impl_->cond, nullptr) != 0) {
        pthread_mutex_destroy(&impl_->mutex);
        return;
    }
    impl_->signaled = 0;
    impl_->inited = true;
}

Event::~Event()
{
    if (!impl_ || !impl_->inited) {
        return;
    }
    pthread_cond_destroy(&impl_->cond);
    pthread_mutex_destroy(&impl_->mutex);
    impl_->inited = false;
}

Result Event::set()
{
    if (!impl_ || !impl_->inited) {
        return kErrInvalidArg;
    }
    if (pthread_mutex_lock(&impl_->mutex) != 0) {
        return kErrInternal;
    }
    impl_->signaled = 1;
    pthread_cond_signal(&impl_->cond);
    pthread_mutex_unlock(&impl_->mutex);
    return kOk;
}

Result Event::reset()
{
    if (!impl_ || !impl_->inited) {
        return kErrInvalidArg;
    }
    if (pthread_mutex_lock(&impl_->mutex) != 0) {
        return kErrInternal;
    }
    impl_->signaled = 0;
    pthread_mutex_unlock(&impl_->mutex);
    return kOk;
}

Result Event::wait()
{
    return wait_timeout_ms(-1);
}

Result Event::wait_timeout_ms(int timeout_ms)
{
    if (!impl_ || !impl_->inited) {
        return kErrInvalidArg;
    }
    if (pthread_mutex_lock(&impl_->mutex) != 0) {
        return kErrInternal;
    }

    if (timeout_ms < 0) {
        while (!impl_->signaled) {
            const int rc = pthread_cond_wait(&impl_->cond, &impl_->mutex);
            if (rc != 0) {
                pthread_mutex_unlock(&impl_->mutex);
                return kErrInternal;
            }
        }
    } else {
        timespec deadline {};
        const Result dr = detail::monotonic_deadline_from_timeout_ms(timeout_ms, &deadline);
        if (dr != kOk) {
            pthread_mutex_unlock(&impl_->mutex);
            return dr;
        }
        while (!impl_->signaled) {
            const int rc = pthread_cond_timedwait(&impl_->cond, &impl_->mutex, &deadline);
            if (rc == ETIMEDOUT) {
                pthread_mutex_unlock(&impl_->mutex);
                return kErrTimeout;
            }
            if (rc != 0) {
                pthread_mutex_unlock(&impl_->mutex);
                return kErrInternal;
            }
        }
    }

    impl_->signaled = 0;
    pthread_mutex_unlock(&impl_->mutex);
    return kOk;
}

}  // namespace openember::osal
