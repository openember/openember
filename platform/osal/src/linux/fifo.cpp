#define _GNU_SOURCE

#include "openember/osal/fifo.hpp"

#include <fcntl.h>
#include <poll.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>

namespace openember::osal {

struct Fifo::Impl {
    int fd = -1;
    bool inited = false;
    uint32_t mode = 0;
    char path[256] {};
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

Fifo::Fifo() : impl_(std::make_unique<Impl>()) {}

Fifo::~Fifo()
{
    (void)close();
}

Result Fifo::query_caps(FifoCaps* out_caps)
{
    if (!out_caps) {
        return kErrInvalidArg;
    }
    out_caps->supports_fifo = 1;
    return kOk;
}

Result Fifo::unlink(const std::string& path)
{
    if (path.empty()) {
        return kErrInvalidArg;
    }
    if (::unlink(path.c_str()) != 0) {
        return errno == ENOENT ? kOk : kErrInternal;
    }
    return kOk;
}

Result Fifo::open(const std::string& path, FifoMode mode)
{
    const uint32_t mode_bits = static_cast<uint32_t>(mode);
    if (path.empty() || (mode_bits & (static_cast<uint32_t>(FifoMode::Read) | static_cast<uint32_t>(FifoMode::Write))) == 0) {
        return kErrInvalidArg;
    }

    impl_ = std::make_unique<Impl>();
    impl_->mode = mode_bits;
    std::strncpy(impl_->path, path.c_str(), sizeof(impl_->path) - 1);

    struct stat st {};
    if (stat(path.c_str(), &st) != 0) {
        if (errno == ENOENT) {
            if (mkfifo(path.c_str(), 0666) != 0) {
                return kErrInternal;
            }
        } else {
            return kErrInternal;
        }
    }

    const int fd = ::open(path.c_str(), O_RDWR | O_NONBLOCK);
    if (fd < 0) {
        return kErrInternal;
    }
    if (set_nonblocking(fd) != kOk) {
        ::close(fd);
        return kErrInternal;
    }

    impl_->fd = fd;
    impl_->inited = true;
    return kOk;
}

Result Fifo::close()
{
    if (!impl_) {
        return kErrInvalidArg;
    }
    const int fd = impl_->fd;
    impl_ = std::make_unique<Impl>();
    if (fd >= 0) {
        ::close(fd);
    }
    return kOk;
}

Result Fifo::write(const void* buf, size_t len, size_t* out_written, int timeout_ms)
{
    if (!impl_ || !impl_->inited || impl_->fd < 0 || (!buf && len > 0)) {
        return kErrInvalidArg;
    }
    if ((impl_->mode & static_cast<uint32_t>(FifoMode::Write)) == 0) {
        return kErrUnsupported;
    }

    const auto* ptr = static_cast<const uint8_t*>(buf);
    size_t total = 0;
    if (out_written) {
        *out_written = 0;
    }

    while (total < len) {
        pollfd fds[1] = {{impl_->fd, POLLOUT, 0}};
        const int rc = poll(fds, 1, timeout_ms);
        if (rc < 0) {
            return errno == EINTR ? kErrAgain : kErrInternal;
        }
        if (rc == 0) {
            return kErrTimeout;
        }

        const ssize_t n = ::write(impl_->fd, ptr + total, len - total);
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

Result Fifo::read(void* buf, size_t len, size_t* out_read, int timeout_ms)
{
    if (!impl_ || !impl_->inited || impl_->fd < 0 || (!buf && len > 0)) {
        return kErrInvalidArg;
    }
    if ((impl_->mode & static_cast<uint32_t>(FifoMode::Read)) == 0) {
        return kErrUnsupported;
    }

    auto* ptr = static_cast<uint8_t*>(buf);
    size_t total = 0;
    if (out_read) {
        *out_read = 0;
    }

    while (total < len) {
        pollfd fds[1] = {{impl_->fd, POLLIN, 0}};
        const int rc = poll(fds, 1, timeout_ms);
        if (rc < 0) {
            return errno == EINTR ? kErrAgain : kErrInternal;
        }
        if (rc == 0) {
            return kErrTimeout;
        }

        const ssize_t n = ::read(impl_->fd, ptr + total, len - total);
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
