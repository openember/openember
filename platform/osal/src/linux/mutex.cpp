#define _POSIX_C_SOURCE 200809L

#include "openember/osal/mutex.hpp"

#include <cstring>
#include <pthread.h>

#include "openember/osal/detail/error.hpp"

namespace openember::osal {

struct Mutex::Impl {
    pthread_mutex_t native {};
    bool inited = false;
};

Mutex::Mutex() : impl_(std::make_unique<Impl>()) {
    if (pthread_mutex_init(&impl_->native, nullptr) == 0) {
        impl_->inited = true;
    }
}

Mutex::~Mutex() {
    if (impl_ && impl_->inited) {
        pthread_mutex_destroy(&impl_->native);
        impl_->inited = false;
    }
}

Result Mutex::lock() {
    if (!impl_ || !impl_->inited) {
        return kErrInvalidArg;
    }
    const int rc = pthread_mutex_lock(&impl_->native);
    if (rc == 0) {
        return kOk;
    }
    return detail::from_errno(rc);
}

Result Mutex::try_lock() {
    if (!impl_ || !impl_->inited) {
        return kErrInvalidArg;
    }
    const int rc = pthread_mutex_trylock(&impl_->native);
    if (rc == 0) {
        return kOk;
    }
    return detail::from_errno(rc);
}

Result Mutex::unlock() {
    if (!impl_ || !impl_->inited) {
        return kErrInvalidArg;
    }
    return pthread_mutex_unlock(&impl_->native) == 0 ? kOk : kErrInternal;
}

pthread_mutex_t* Mutex::native_handle() noexcept
{
    return impl_ && impl_->inited ? &impl_->native : nullptr;
}

MutexLock::MutexLock(Mutex& mutex) : mutex_(mutex) {
    result_ = mutex_.lock();
    locked_ = (result_ == kOk);
}

MutexLock::~MutexLock() {
    if (locked_) {
        (void)mutex_.unlock();
    }
}

}  // namespace openember::osal
