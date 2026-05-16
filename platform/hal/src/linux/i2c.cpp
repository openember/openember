#define _GNU_SOURCE

#include "openember/hal/i2c.hpp"

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <linux/i2c-dev.h>
#include <linux/i2c.h>
#include <sys/ioctl.h>
#include <unistd.h>

namespace openember::hal {

struct I2c::Impl {
    int fd = -1;
    uint8_t addr = 0;
    bool inited = false;
    uint32_t bus_num = 0;
};

I2c::I2c() : impl_(std::make_unique<Impl>()) {}

I2c::~I2c()
{
    (void)close();
}

Result I2c::query_caps(I2cCaps* out_caps)
{
    if (!out_caps) {
        return osal::kErrInvalidArg;
    }

    out_caps->max_write_bytes = 0;
    out_caps->max_read_bytes = 0;
    out_caps->supports_write_read = 1;
    return osal::kOk;
}

Result I2c::open(uint32_t bus_num, uint8_t addr_7bit)
{
    (void)close();

    char path[64];
    std::snprintf(path, sizeof(path), "/dev/i2c-%u", bus_num);

    const int fd = ::open(path, O_RDWR);
    if (fd < 0) {
        return osal::kErrInternal;
    }

    if (ioctl(fd, I2C_SLAVE, static_cast<int>(addr_7bit)) < 0) {
        ::close(fd);
        return osal::kErrUnsupported;
    }

    impl_->fd = fd;
    impl_->addr = addr_7bit;
    impl_->bus_num = bus_num;
    impl_->inited = true;
    return osal::kOk;
}

Result I2c::close()
{
    const int fd = impl_->fd;
    impl_->fd = -1;
    impl_->inited = false;
    impl_->addr = 0;
    impl_->bus_num = 0;

    if (fd > 0) {
        return ::close(fd) == 0 ? osal::kOk : osal::kErrInternal;
    }
    return osal::kOk;
}

Result I2c::write(const uint8_t* data, size_t len, size_t* out_written)
{
    if (!data && len > 0) {
        return osal::kErrInvalidArg;
    }

    if (!impl_->inited || impl_->fd < 0) {
        return osal::kErrInvalidArg;
    }

    size_t total = 0;
    while (total < len) {
        const ssize_t n = ::write(impl_->fd, data + total, len - total);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            return osal::kErrInternal;
        }
        total += static_cast<size_t>(n);
    }

    if (out_written) {
        *out_written = total;
    }
    return osal::kOk;
}

Result I2c::read(uint8_t* data, size_t len, size_t* out_read)
{
    if (!data && len > 0) {
        return osal::kErrInvalidArg;
    }

    if (!impl_->inited || impl_->fd < 0) {
        return osal::kErrInvalidArg;
    }

    size_t total = 0;
    while (total < len) {
        const ssize_t n = ::read(impl_->fd, data + total, len - total);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            return osal::kErrInternal;
        }
        if (n == 0) {
            break;
        }
        total += static_cast<size_t>(n);
    }

    if (out_read) {
        *out_read = total;
    }
    return total == len ? osal::kOk : osal::kErrInternal;
}

Result I2c::write_read(const uint8_t* wbuf, size_t wlen, uint8_t* rbuf, size_t rlen)
{
    if ((!wbuf && wlen > 0) || (!rbuf && rlen > 0)) {
        return osal::kErrInvalidArg;
    }

    if (!impl_->inited || impl_->fd < 0) {
        return osal::kErrInvalidArg;
    }

    i2c_rdwr_ioctl_data rdwr {};
    i2c_msg msgs[2] {};

    msgs[0].addr = impl_->addr;
    msgs[0].flags = 0;
    msgs[0].len = static_cast<uint16_t>(wlen);
    msgs[0].buf = const_cast<uint8_t*>(wbuf);

    msgs[1].addr = impl_->addr;
    msgs[1].flags = I2C_M_RD;
    msgs[1].len = static_cast<uint16_t>(rlen);
    msgs[1].buf = rbuf;

    rdwr.msgs = msgs;
    rdwr.nmsgs = (wlen > 0) ? 2 : 1;
    if (wlen == 0) {
        rdwr.msgs = &msgs[1];
        rdwr.nmsgs = 1;
    }

    return ioctl(impl_->fd, I2C_RDWR, &rdwr) < 0 ? osal::kErrInternal : osal::kOk;
}

}  // namespace openember::hal
