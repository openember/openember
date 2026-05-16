#define _POSIX_C_SOURCE 200809L

#include "openember/hal/gpio.hpp"

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>

namespace openember::hal {

struct Gpio::Impl {
    uint32_t gpio_num = 0;
    bool inited = false;
    bool exported_by_us = false;
    int dir_fd = -1;
    int value_fd = -1;
};

namespace {

Result sysfs_export(uint32_t gpio_num)
{
    const int fd = ::open("/sys/class/gpio/export", O_WRONLY);
    if (fd < 0) {
        return osal::kErrUnsupported;
    }

    char buf[32];
    const int n = std::snprintf(buf, sizeof(buf), "%u", gpio_num);
    if (n < 0) {
        ::close(fd);
        return osal::kErrInternal;
    }

    if (::write(fd, buf, static_cast<size_t>(n)) < 0) {
        if (errno == EBUSY || errno == EINVAL) {
            ::close(fd);
            return osal::kOk;
        }
        ::close(fd);
        return osal::kErrInternal;
    }

    ::close(fd);
    return osal::kOk;
}

Result sysfs_try_open_paths(uint32_t gpio_num, int* out_dir_fd, int* out_value_fd)
{
    char dir_path[128];
    char value_path[128];

    std::snprintf(dir_path, sizeof(dir_path), "/sys/class/gpio/gpio%u/direction", gpio_num);
    std::snprintf(value_path, sizeof(value_path), "/sys/class/gpio/gpio%u/value", gpio_num);

    const int dir_fd = ::open(dir_path, O_RDWR);
    if (dir_fd < 0) {
        return osal::kErrInternal;
    }

    const int value_fd = ::open(value_path, O_RDWR);
    if (value_fd < 0) {
        ::close(dir_fd);
        return osal::kErrInternal;
    }

    *out_dir_fd = dir_fd;
    *out_value_fd = value_fd;
    return osal::kOk;
}

Result write_string_fd(int fd, const char* s)
{
    if (lseek(fd, 0, SEEK_SET) < 0) {
        return osal::kErrInternal;
    }

    const size_t len = std::strlen(s);
    if (::write(fd, s, len) != static_cast<ssize_t>(len)) {
        return osal::kErrInternal;
    }
    return osal::kOk;
}

}  // namespace

Gpio::Gpio() : impl_(std::make_unique<Impl>()) {}

Gpio::~Gpio()
{
    (void)close();
}

Result Gpio::query_caps(GpioCaps* out_caps)
{
    if (!out_caps) {
        return osal::kErrInvalidArg;
    }

    out_caps->direction_mask = static_cast<uint32_t>(GpioCapsFlag::DirIn) |
                               static_cast<uint32_t>(GpioCapsFlag::DirOut);
    return osal::kOk;
}

Result Gpio::open(uint32_t gpio_num)
{
    (void)close();

    impl_->gpio_num = gpio_num;

    int dir_fd = -1;
    int value_fd = -1;

    Result r = sysfs_try_open_paths(gpio_num, &dir_fd, &value_fd);
    if (r != osal::kOk) {
        r = sysfs_export(gpio_num);
        if (r != osal::kOk) {
            return r;
        }
        r = sysfs_try_open_paths(gpio_num, &dir_fd, &value_fd);
        if (r != osal::kOk) {
            return r;
        }
        impl_->exported_by_us = true;
    }

    impl_->dir_fd = dir_fd;
    impl_->value_fd = value_fd;
    impl_->inited = true;
    return osal::kOk;
}

Result Gpio::set_direction(GpioDirection dir)
{
    if (!impl_->inited || impl_->dir_fd < 0) {
        return osal::kErrInvalidArg;
    }

    switch (dir) {
    case GpioDirection::In:
        return write_string_fd(impl_->dir_fd, "in");
    case GpioDirection::Out:
        return write_string_fd(impl_->dir_fd, "out");
    default:
        return osal::kErrInvalidArg;
    }
}

Result Gpio::read(uint8_t* out_value)
{
    if (!out_value) {
        return osal::kErrInvalidArg;
    }

    if (!impl_->inited || impl_->value_fd < 0) {
        return osal::kErrInvalidArg;
    }

    if (lseek(impl_->value_fd, 0, SEEK_SET) < 0) {
        return osal::kErrInternal;
    }

    char ch = 0;
    const ssize_t n = ::read(impl_->value_fd, &ch, 1);
    if (n <= 0) {
        return osal::kErrInternal;
    }

    *out_value = (ch == '0') ? 0 : 1;
    return osal::kOk;
}

Result Gpio::write(uint8_t value)
{
    if (!impl_->inited || impl_->value_fd < 0) {
        return osal::kErrInvalidArg;
    }

    const char ch = (value == 0) ? '0' : '1';

    if (lseek(impl_->value_fd, 0, SEEK_SET) < 0) {
        return osal::kErrInternal;
    }

    if (::write(impl_->value_fd, &ch, 1) != 1) {
        return osal::kErrInternal;
    }
    return osal::kOk;
}

Result Gpio::close()
{
    const int dir_fd = impl_->dir_fd;
    const int value_fd = impl_->value_fd;

    impl_->dir_fd = -1;
    impl_->value_fd = -1;
    impl_->inited = false;
    impl_->exported_by_us = false;
    impl_->gpio_num = 0;

    if (dir_fd >= 0) {
        ::close(dir_fd);
    }
    if (value_fd >= 0) {
        ::close(value_fd);
    }
    return osal::kOk;
}

}  // namespace openember::hal
