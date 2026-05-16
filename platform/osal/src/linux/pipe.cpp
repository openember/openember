#define _GNU_SOURCE

#include "openember/osal/pipe.hpp"

#include <fcntl.h>
#include <poll.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>

namespace openember::osal {

struct Pipe::Impl {
    int fds[2] = {-1, -1};
    bool inited = false;
};

namespace {

Result set_nonblocking(int fd)
{
    const int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) {
        return kErrInternal;
    }
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK) == 0 ? kOk : kErrInternal;
}

}  // namespace

Pipe::Pipe() : impl_(std::make_unique<Impl>()) {}

Pipe::~Pipe()
{
    (void)close();
}

Result Pipe::query_caps(PipeCaps* out_caps)
{
    if (!out_caps) {
        return kErrInvalidArg;
    }
    out_caps->supports_pipe = 1;
    return kOk;
}

Result Pipe::create()
{
    int fds[2] = {-1, -1};
    if (::pipe(fds) != 0) {
        return kErrInternal;
    }
    if (set_nonblocking(fds[0]) != kOk || set_nonblocking(fds[1]) != kOk) {
        ::close(fds[0]);
        ::close(fds[1]);
        return kErrInternal;
    }
    impl_->fds[0] = fds[0];
    impl_->fds[1] = fds[1];
    impl_->inited = true;
    return kOk;
}

Result Pipe::close()
{
    if (!impl_) {
        return kErrInvalidArg;
    }
    const int r0 = impl_->fds[0];
    const int r1 = impl_->fds[1];
    impl_ = std::make_unique<Impl>();
    if (r0 >= 0) {
        ::close(r0);
    }
    if (r1 >= 0) {
        ::close(r1);
    }
    return kOk;
}

Result Pipe::write(const void* buf, size_t len, size_t* out_written, int timeout_ms)
{
    if (!impl_ || !impl_->inited || (!buf && len > 0)) {
        return kErrInvalidArg;
    }

    const auto* ptr = static_cast<const uint8_t*>(buf);
    size_t total = 0;
    if (out_written) {
        *out_written = 0;
    }

    while (total < len) {
        pollfd fds[1] = {{impl_->fds[1], POLLOUT, 0}};
        const int rc = poll(fds, 1, timeout_ms);
        if (rc < 0) {
            return errno == EINTR ? kErrAgain : kErrInternal;
        }
        if (rc == 0) {
            return kErrTimeout;
        }

        const ssize_t n = ::write(impl_->fds[1], ptr + total, len - total);
        if (n > 0) {
            total += static_cast<size_t>(n);
            continue;
        }
        if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            if (timeout_ms == 0) {
                return kErrTimeout;
            }
            continue;
        }
        if (n < 0 && errno == EINTR) {
            continue;
        }
        return kErrInternal;
    }

    if (out_written) {
        *out_written = total;
    }
    return kOk;
}

Result Pipe::read(void* buf, size_t len, size_t* out_read, int timeout_ms)
{
    if (!impl_ || !impl_->inited || (!buf && len > 0)) {
        return kErrInvalidArg;
    }

    auto* ptr = static_cast<uint8_t*>(buf);
    size_t total = 0;
    if (out_read) {
        *out_read = 0;
    }

    while (total < len) {
        pollfd fds[1] = {{impl_->fds[0], POLLIN, 0}};
        const int rc = poll(fds, 1, timeout_ms);
        if (rc < 0) {
            return errno == EINTR ? kErrAgain : kErrInternal;
        }
        if (rc == 0) {
            return kErrTimeout;
        }

        const ssize_t n = ::read(impl_->fds[0], ptr + total, len - total);
        if (n > 0) {
            total += static_cast<size_t>(n);
            continue;
        }
        if (n == 0) {
            return kErrInternal;
        }
        if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            if (timeout_ms == 0) {
                return kErrTimeout;
            }
            continue;
        }
        if (n < 0 && errno == EINTR) {
            continue;
        }
        return kErrInternal;
    }

    if (out_read) {
        *out_read = total;
    }
    return kOk;
}

}  // namespace openember::osal
