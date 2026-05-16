#define _POSIX_C_SOURCE 200809L

#include "openember/osal/thread.hpp"

#include <pthread.h>

#include "openember/osal/detail/error.hpp"

namespace openember::osal {

struct Thread::Impl {
    pthread_t handle {};
    bool started = false;
    bool joined = false;
};

struct ThreadStart {
    std::shared_ptr<std::function<void()>> fn;
};

static void* thread_trampoline(void* arg)
{
    auto* start = static_cast<ThreadStart*>(arg);
    if (start && start->fn) {
        (*start->fn)();
    }
    delete start;
    return nullptr;
}

Thread::Thread() = default;

Thread::~Thread()
{
    if (impl_ && impl_->started && !impl_->joined) {
        (void)pthread_join(impl_->handle, nullptr);
    }
}

Result Thread::start_impl(std::shared_ptr<std::function<void()>> fn)
{
    if (!fn) {
        return kErrInvalidArg;
    }
    if (impl_) {
        return kErrBusy;
    }

    impl_ = std::make_unique<Impl>();
    auto* start = new ThreadStart{std::move(fn)};
    const int rc = pthread_create(&impl_->handle, nullptr, thread_trampoline, start);
    if (rc != 0) {
        delete start;
        impl_.reset();
        return detail::from_errno(rc);
    }

    impl_->started = true;
    impl_->joined = false;
    return kOk;
}

Result Thread::join()
{
    if (!impl_ || !impl_->started) {
        return kErrInvalidArg;
    }
    if (impl_->joined) {
        return kErrInvalidArg;
    }

    const int rc = pthread_join(impl_->handle, nullptr);
    if (rc != 0) {
        return detail::from_errno(rc);
    }

    impl_->joined = true;
    impl_.reset();
    return kOk;
}

}  // namespace openember::osal
