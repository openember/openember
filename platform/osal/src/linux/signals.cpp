#define _GNU_SOURCE

#include "openember/osal/signals.hpp"

#include <poll.h>
#include <signal.h>
#include <sys/signalfd.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>

namespace openember::osal {

struct SignalWaiter::Impl {
    int fd = -1;
    bool inited = false;
    sigset_t oldmask {};
    bool has_oldmask = false;
};

SignalWaiter::SignalWaiter() : impl_(std::make_unique<Impl>()) {}

SignalWaiter::~SignalWaiter()
{
    (void)close();
}

Result SignalWaiter::query_caps(SignalsCaps* out_caps)
{
    if (!out_caps) {
        return kErrInvalidArg;
    }
    out_caps->supports_signalfd = 1;
    return kOk;
}

Result SignalWaiter::open(const std::vector<int>& signums)
{
    if (signums.empty()) {
        return kErrInvalidArg;
    }

    impl_ = std::make_unique<Impl>();
    sigset_t mask {};
    sigemptyset(&mask);
    for (const int signum : signums) {
        if (signum <= 0) {
            return kErrInvalidArg;
        }
        sigaddset(&mask, signum);
    }

    if (sigprocmask(SIG_BLOCK, &mask, &impl_->oldmask) != 0) {
        return kErrInternal;
    }
    impl_->has_oldmask = true;

    const int fd = signalfd(-1, &mask, SFD_NONBLOCK | SFD_CLOEXEC);
    if (fd < 0) {
        if (impl_->has_oldmask) {
            sigprocmask(SIG_SETMASK, &impl_->oldmask, nullptr);
        }
        return kErrInternal;
    }

    impl_->fd = fd;
    impl_->inited = true;
    return kOk;
}

Result SignalWaiter::wait(SignalInfo* out_info, int timeout_ms)
{
    if (!impl_ || !impl_->inited || impl_->fd < 0 || !out_info) {
        return kErrInvalidArg;
    }

    pollfd fds[1] = {{impl_->fd, POLLIN, 0}};
    const int rc = poll(fds, 1, timeout_ms);
    if (rc < 0) {
        return errno == EINTR ? kErrAgain : kErrInternal;
    }
    if (rc == 0) {
        return kErrTimeout;
    }

    signalfd_siginfo info {};
    const ssize_t n = ::read(impl_->fd, &info, sizeof(info));
    if (n != static_cast<ssize_t>(sizeof(info))) {
        if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            return kErrTimeout;
        }
        return kErrInternal;
    }

    out_info->signum = static_cast<int>(info.ssi_signo);
    out_info->sender_pid = static_cast<int>(info.ssi_pid);
    return kOk;
}

Result SignalWaiter::close()
{
    if (!impl_) {
        return kErrInvalidArg;
    }

    const int fd = impl_->fd;
    const bool has_oldmask = impl_->has_oldmask;
    const sigset_t oldmask = impl_->oldmask;
    impl_ = std::make_unique<Impl>();

    if (fd >= 0) {
        ::close(fd);
    }
    if (has_oldmask) {
        sigprocmask(SIG_SETMASK, &oldmask, nullptr);
    }
    return kOk;
}

}  // namespace openember::osal
