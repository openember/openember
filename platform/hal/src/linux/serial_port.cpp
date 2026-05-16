#define _GNU_SOURCE

#include "openember/hal/serial_port.hpp"

#include <cerrno>
#include <fcntl.h>
#include <unistd.h>

#ifdef __linux__
#include <asm/termbits.h>
#include <sys/ioctl.h>
#else
#include <termios.h>
#endif

namespace openember::hal {

struct SerialPort::Impl {
    int fd = -1;
    bool inited = false;
};

bool is_standard_serial_baud(uint32_t baud_rate)
{
    for (const SerialBaud b : kCommonSerialBaudRates) {
        if (baud_rate == static_cast<uint32_t>(b)) {
            return true;
        }
    }
    return false;
}

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
        return static_cast<speed_t>(-1);
    }
}

#ifdef __linux__
void apply_frame_format(termios2& tio, const SerialPortConfig& cfg)
{
    tio.c_iflag &=
        ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON | IXOFF | IXANY);
    tio.c_oflag &= ~OPOST;
    tio.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
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
    case SerialParity::None:
        break;
    case SerialParity::Even:
        tio.c_cflag |= PARENB;
        tio.c_cflag &= ~PARODD;
        break;
    case SerialParity::Odd:
        tio.c_cflag |= PARENB | PARODD;
        break;
    default:
        break;
    }
}

Result apply_line_settings(int fd, const SerialPortConfig& cfg)
{
    if (cfg.baud_rate == 0) {
        return osal::kErrInvalidArg;
    }

    termios2 tio {};
    if (ioctl(fd, TCGETS2, &tio) != 0) {
        return osal::kErrInternal;
    }

    apply_frame_format(tio, cfg);
    tio.c_cflag &= ~static_cast<tcflag_t>(CBAUD);

    const speed_t speed = baud_to_flag(cfg.baud_rate);
    if (speed != static_cast<speed_t>(-1)) {
        tio.c_cflag |= speed;
    } else {
        tio.c_cflag |= BOTHER;
    }
    tio.c_ispeed = cfg.baud_rate;
    tio.c_ospeed = cfg.baud_rate;

    return ioctl(fd, TCSETS2, &tio) == 0 ? osal::kOk : osal::kErrInternal;
}

#else

void apply_frame_format(termios& tio, const SerialPortConfig& cfg)
{
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
    case SerialParity::None:
        break;
    case SerialParity::Even:
        tio.c_cflag |= PARENB;
        tio.c_cflag &= ~PARODD;
        break;
    case SerialParity::Odd:
        tio.c_cflag |= PARENB | PARODD;
        break;
    default:
        break;
    }
}

Result apply_line_settings(int fd, const SerialPortConfig& cfg)
{
    if (cfg.baud_rate == 0) {
        return osal::kErrInvalidArg;
    }

    const speed_t speed = baud_to_flag(cfg.baud_rate);
    if (speed == static_cast<speed_t>(-1)) {
        return osal::kErrUnsupported;
    }

    termios tio {};
    if (tcgetattr(fd, &tio) != 0) {
        return osal::kErrInternal;
    }

    apply_frame_format(tio, cfg);
    cfsetispeed(&tio, speed);
    cfsetospeed(&tio, speed);

    return tcsetattr(fd, TCSANOW, &tio) == 0 ? osal::kOk : osal::kErrInternal;
}

#endif

}  // namespace

SerialPort::SerialPort() : impl_(std::make_unique<Impl>()) {}

SerialPort::~SerialPort()
{
    (void)close();
}

Result SerialPort::query_caps(SerialPortCaps* out_caps)
{
    if (!out_caps) {
        return osal::kErrInvalidArg;
    }

    out_caps->baud_rate_count = static_cast<uint32_t>(kCommonSerialBaudRates.size());
    for (uint32_t i = 0; i < out_caps->baud_rate_count; i++) {
        out_caps->baud_rates[i] = static_cast<uint32_t>(kCommonSerialBaudRates[i]);
    }

    out_caps->parity_mask = static_cast<uint32_t>(SerialParityMask::None) |
                            static_cast<uint32_t>(SerialParityMask::Even) |
                            static_cast<uint32_t>(SerialParityMask::Odd);
    out_caps->data_bits_min = 7;
    out_caps->data_bits_max = 8;
    out_caps->stop_bits_min = 1;
    out_caps->stop_bits_max = 2;
    return osal::kOk;
}

Result SerialPort::open(const std::string& path, const SerialPortConfig& cfg)
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
    if (cfg.parity != SerialParity::None && cfg.parity != SerialParity::Even &&
        cfg.parity != SerialParity::Odd) {
        return osal::kErrInvalidArg;
    }

    (void)close();

    const int fd = ::open(path.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (fd < 0) {
        return osal::kErrInternal;
    }

    const Result r = apply_line_settings(fd, cfg);
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

Result SerialPort::close()
{
    const int fd = impl_->fd;
    impl_->fd = -1;
    impl_->inited = false;

    if (fd > 0) {
        return ::close(fd) == 0 ? osal::kOk : osal::kErrInternal;
    }
    return osal::kOk;
}

bool SerialPort::is_open() const
{
    return impl_ && impl_->inited && impl_->fd >= 0;
}

Result SerialPort::read(void* buf, size_t len, size_t* out_read)
{
    if (!buf && len > 0) {
        return osal::kErrInvalidArg;
    }

    if (!is_open()) {
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

Result SerialPort::write(const void* buf, size_t len, size_t* out_written)
{
    if (!buf && len > 0) {
        return osal::kErrInvalidArg;
    }

    if (!is_open()) {
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
