#define _GNU_SOURCE

#include "openember/hal/uart.hpp"

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>

namespace openember::hal {

struct Uart::Impl {
    int fd = -1;
    bool inited = false;
};

namespace {

speed_t baud_to_flag(uint32_t baud)
{
    switch (baud) {
    case 9600:
        return B9600;
    case 19200:
        return B19200;
    case 38400:
        return B38400;
    case 57600:
        return B57600;
    case 115200:
        return B115200;
    case 230400:
        return B230400;
    case 460800:
        return B460800;
    case 921600:
        return B921600;
    default:
        return B115200;
    }
}

Result apply_termios(int fd, const UartConfig& cfg)
{
    termios tio {};
    if (tcgetattr(fd, &tio) != 0) {
        return osal::kErrInternal;
    }

    cfmakeraw(&tio);
    tio.c_cflag |= CLOCAL | CREAD;

    tio.c_cflag &= ~(CSIZE | PARENB | PARODD | CSTOPB);
    if (cfg.data_bits == 7) {
        tio.c_cflag |= CS7;
    } else {
        tio.c_cflag |= CS8;
    }

    if (cfg.stop_bits == 2) {
        tio.c_cflag |= CSTOPB;
    }

    switch (cfg.parity) {
    case UartParity::None:
        break;
    case UartParity::Even:
        tio.c_cflag |= PARENB;
        tio.c_cflag &= ~PARODD;
        break;
    case UartParity::Odd:
        tio.c_cflag |= PARENB | PARODD;
        break;
    default:
        return osal::kErrInvalidArg;
    }

    cfsetispeed(&tio, baud_to_flag(cfg.baud_rate));
    cfsetospeed(&tio, baud_to_flag(cfg.baud_rate));

    return tcsetattr(fd, TCSANOW, &tio) == 0 ? osal::kOk : osal::kErrInternal;
}

}  // namespace

Uart::Uart() : impl_(std::make_unique<Impl>()) {}

Uart::~Uart()
{
    (void)close();
}

Result Uart::query_caps(UartCaps* out_caps)
{
    static const uint32_t rates[] = {9600, 19200, 38400, 57600, 115200, 230400, 460800, 921600};

    if (!out_caps) {
        return osal::kErrInvalidArg;
    }

    out_caps->baud_rate_count = static_cast<uint32_t>(sizeof(rates) / sizeof(rates[0]));
    if (out_caps->baud_rate_count > kUartCapsMaxBaudRates) {
        out_caps->baud_rate_count = kUartCapsMaxBaudRates;
    }

    for (uint32_t i = 0; i < out_caps->baud_rate_count; i++) {
        out_caps->baud_rates[i] = rates[i];
    }

    out_caps->parity_mask = static_cast<uint32_t>(UartParityMask::None) |
                            static_cast<uint32_t>(UartParityMask::Even) |
                            static_cast<uint32_t>(UartParityMask::Odd);
    out_caps->data_bits_min = 7;
    out_caps->data_bits_max = 8;
    out_caps->stop_bits_min = 1;
    out_caps->stop_bits_max = 2;
    return osal::kOk;
}

Result Uart::open(const std::string& path, const UartConfig& cfg)
{
    if (path.empty()) {
        return osal::kErrInvalidArg;
    }

    if (cfg.data_bits != 7 && cfg.data_bits != 8) {
        return osal::kErrInvalidArg;
    }
    if (cfg.stop_bits != 1 && cfg.stop_bits != 2) {
        return osal::kErrInvalidArg;
    }

    (void)close();

    const int fd = ::open(path.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (fd < 0) {
        return osal::kErrInternal;
    }

    const Result r = apply_termios(fd, cfg);
    if (r != osal::kOk) {
        ::close(fd);
        return r;
    }

    if (cfg.nonblocking == 0) {
        const int fl = fcntl(fd, F_GETFL, 0);
        if (fl >= 0) {
            (void)fcntl(fd, F_SETFL, fl & ~O_NONBLOCK);
        }
    }

    impl_->fd = fd;
    impl_->inited = true;
    return osal::kOk;
}

Result Uart::close()
{
    const int fd = impl_->fd;
    impl_->fd = -1;
    impl_->inited = false;

    if (fd > 0) {
        return ::close(fd) == 0 ? osal::kOk : osal::kErrInternal;
    }
    return osal::kOk;
}

Result Uart::read(void* buf, size_t len, size_t* out_read)
{
    if (!buf && len > 0) {
        return osal::kErrInvalidArg;
    }

    if (!impl_->inited || impl_->fd < 0) {
        return osal::kErrInvalidArg;
    }

    if (len == 0) {
        if (out_read) {
            *out_read = 0;
        }
        return osal::kOk;
    }

    ssize_t n = 0;
    do {
        n = ::read(impl_->fd, buf, len);
    } while (n < 0 && errno == EINTR);

    if (n < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            if (out_read) {
                *out_read = 0;
            }
            return osal::kErrAgain;
        }
        return osal::kErrInternal;
    }

    if (out_read) {
        *out_read = static_cast<size_t>(n);
    }
    return osal::kOk;
}

Result Uart::write(const void* buf, size_t len, size_t* out_written)
{
    if (!buf && len > 0) {
        return osal::kErrInvalidArg;
    }

    if (!impl_->inited || impl_->fd < 0) {
        return osal::kErrInvalidArg;
    }

    size_t total = 0;
    const auto* p = static_cast<const uint8_t*>(buf);

    while (total < len) {
        ssize_t n = 0;
        do {
            n = ::write(impl_->fd, p + total, len - total);
        } while (n < 0 && errno == EINTR);

        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                if (out_written) {
                    *out_written = total;
                }
                return total > 0 ? osal::kOk : osal::kErrAgain;
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

}  // namespace openember::hal
