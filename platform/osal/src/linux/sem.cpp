#define _POSIX_C_SOURCE 200809L

#include "openember/osal/sem.hpp"

#include <cerrno>
#include <semaphore.h>

#include "openember/osal/detail/deadline.hpp"
#include "openember/osal/detail/error.hpp"

namespace openember::osal {

struct Semaphore::Impl {
    sem_t native {};
    bool inited = false;
};

Semaphore::Semaphore(uint32_t initial_count) : impl_(std::make_unique<Impl>())
{
    if (sem_init(&impl_->native, 0, initial_count) == 0) {
        impl_->inited = true;
    }
}

Semaphore::~Semaphore()
{
    if (impl_ && impl_->inited) {
        sem_destroy(&impl_->native);
        impl_->inited = false;
    }
}

Result Semaphore::post()
{
    if (!impl_ || !impl_->inited) {
        return kErrInvalidArg;
    }
    return sem_post(&impl_->native) == 0 ? kOk : kErrInternal;
}

Result Semaphore::wait()
{
    if (!impl_ || !impl_->inited) {
        return kErrInvalidArg;
    }
    int rc = 0;
    do {
        rc = sem_wait(&impl_->native);
    } while (rc != 0 && errno == EINTR);
    return rc == 0 ? kOk : kErrInternal;
}

Result Semaphore::try_wait()
{
    if (!impl_ || !impl_->inited) {
        return kErrInvalidArg;
    }
    if (sem_trywait(&impl_->native) == 0) {
        return kOk;
    }
    if (errno == EAGAIN || errno == EINTR) {
        return kErrAgain;
    }
    return kErrInternal;
}

Result Semaphore::wait_timeout_ms(int timeout_ms)
{
    if (!impl_ || !impl_->inited) {
        return kErrInvalidArg;
    }
    if (timeout_ms < 0) {
        return wait();
    }
    if (timeout_ms == 0) {
        return try_wait();
    }

    timespec deadline {};
    const Result dr = detail::realtime_deadline_from_timeout_ms(timeout_ms, &deadline);
    if (dr != kOk) {
        return dr;
    }

    int rc = 0;
    do {
        rc = sem_timedwait(&impl_->native, &deadline);
    } while (rc != 0 && errno == EINTR);

    if (rc == 0) {
        return kOk;
    }
    if (errno == ETIMEDOUT) {
        return kErrTimeout;
    }
    return kErrInternal;
}

}  // namespace openember::osal
