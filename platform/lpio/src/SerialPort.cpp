#include "lpio/SerialPort.hpp"

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>

#ifdef __linux__
#include <asm/termbits.h>
#include <sys/ioctl.h>
#endif

namespace lpio {

namespace {

[[noreturn]] void throwErrno(std::string_view context)
{
    throw DeviceError(errno, context);
}

#ifdef __linux__
speed_t baudToFlag(uint32_t baud)
{
    switch (baud) {
    case 9600: return B9600;
    case 19200: return B19200;
    case 38400: return B38400;
    case 57600: return B57600;
    case 115200: return B115200;
    case 230400: return B230400;
    case 460800: return B460800;
    case 921600: return B921600;
    default: return static_cast<speed_t>(-1);
    }
}

void applyFrameFormat(termios2& tio, const SerialPortConfig& cfg)
{
    tio.c_iflag &=
        ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON | IXOFF | IXANY);
    tio.c_oflag &= ~OPOST;
    tio.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
    tio.c_cflag |= CLOCAL | CREAD;

    tio.c_cflag &= ~(CSIZE | PARENB | PARODD | CSTOPB);
    if (cfg.dataBits == 7) {
        tio.c_cflag |= CS7;
    } else {
        tio.c_cflag |= CS8;
    }

    if (cfg.stopBits == 2) {
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
    }
}

void applyLineSettings(int fd, const SerialPortConfig& cfg)
{
    if (cfg.baudRate == 0) {
        throw DeviceError(std::errc::invalid_argument, "baud rate must be non-zero");
    }

    termios2 tio {};
    if (ioctl(fd, TCGETS2, &tio) != 0) {
        throwErrno("TCGETS2");
    }

    applyFrameFormat(tio, cfg);
    tio.c_cflag &= ~static_cast<tcflag_t>(CBAUD);

    const speed_t speed = baudToFlag(cfg.baudRate);
    if (speed != static_cast<speed_t>(-1)) {
        tio.c_cflag |= speed;
    } else {
        tio.c_cflag |= BOTHER;
    }
    tio.c_ispeed = cfg.baudRate;
    tio.c_ospeed = cfg.baudRate;

    if (ioctl(fd, TCSETS2, &tio) != 0) {
        throwErrno("TCSETS2");
    }
}
#endif

}  // namespace

SerialPort::Builder::Builder(std::string path) : path_(std::move(path)) {}

SerialPort::Builder& SerialPort::Builder::baudRate(uint32_t rate)
{
    cfg_.baudRate = rate;
    return *this;
}

SerialPort::Builder& SerialPort::Builder::baud(SerialBaud rate)
{
    cfg_.baudRate = static_cast<uint32_t>(rate);
    return *this;
}

SerialPort::Builder& SerialPort::Builder::dataBits(uint8_t bits)
{
    cfg_.dataBits = bits;
    return *this;
}

SerialPort::Builder& SerialPort::Builder::stopBits(uint8_t bits)
{
    cfg_.stopBits = bits;
    return *this;
}

SerialPort::Builder& SerialPort::Builder::parity(SerialParity p)
{
    cfg_.parity = p;
    return *this;
}

SerialPort::Builder& SerialPort::Builder::nonBlocking(bool enable)
{
    cfg_.nonBlocking = enable;
    return *this;
}

SerialPort SerialPort::Builder::build()
{
    return SerialPort(path_, cfg_);
}

SerialPort SerialPort::Builder::buildAndOpen(OpenMode mode)
{
    auto dev = build();
    dev.open(mode);
    return dev;
}

SerialPort::Builder SerialPort::on(std::string path)
{
    return Builder(std::move(path));
}

SerialPort::SerialPort(std::string path, SerialPortConfig config)
    : path_(std::move(path))
    , config_(config)
{}

SerialPort::~SerialPort()
{
    close();
}

SerialPort::SerialPort(SerialPort&& other) noexcept
    : path_(std::move(other.path_))
    , config_(other.config_)
    , state_(other.state_)
    , fd_(other.fd_)
{
    other.state_ = DeviceState::Closed;
    other.fd_    = -1;
}

SerialPort& SerialPort::operator=(SerialPort&& other) noexcept
{
    if (this != &other) {
        close();
        path_   = std::move(other.path_);
        config_ = other.config_;
        state_  = other.state_;
        fd_     = other.fd_;
        other.state_ = DeviceState::Closed;
        other.fd_    = -1;
    }
    return *this;
}

void SerialPort::open(OpenMode mode)
{
    (void)mode;
    if (state_ == DeviceState::Open) {
        return;
    }

    if (path_.empty()) {
        throw DeviceError(std::errc::invalid_argument, "serial path is empty");
    }

    const int flags = O_RDWR | O_NOCTTY | (config_.nonBlocking ? O_NONBLOCK : 0);
    const int fd    = ::open(path_.c_str(), flags);
    if (fd < 0) {
        throwErrno(path_);
    }

#ifdef __linux__
    try {
        applyLineSettings(fd, config_);
    } catch (...) {
        ::close(fd);
        throw;
    }
#else
    (void)config_;
    ::close(fd);
    throw DeviceError(std::errc::not_supported, "SerialPort requires Linux termios2");
#endif

    if (!config_.nonBlocking) {
        const int fl = fcntl(fd, F_GETFL, 0);
        if (fl >= 0) {
            (void)fcntl(fd, F_SETFL, fl & ~O_NONBLOCK);
        }
    }

    fd_    = fd;
    state_ = DeviceState::Open;
}

void SerialPort::close() noexcept
{
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
    state_ = DeviceState::Closed;
}

DeviceState SerialPort::state() const noexcept
{
    return state_;
}

std::string_view SerialPort::path() const noexcept
{
    return path_;
}

std::size_t SerialPort::read(std::span<std::byte> buf)
{
    requireOpen();
    if (buf.empty()) {
        return 0;
    }

    ssize_t n = 0;
    do {
        n = ::read(fd_, buf.data(), buf.size());
    } while (n < 0 && errno == EINTR);

    if (n < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return 0;
        }
        throwErrno("serial read");
    }
    return static_cast<std::size_t>(n);
}

std::size_t SerialPort::write(std::span<const std::byte> buf)
{
    requireOpen();
    if (buf.empty()) {
        return 0;
    }

    ssize_t n = 0;
    do {
        n = ::write(fd_, buf.data(), buf.size());
    } while (n < 0 && errno == EINTR);

    if (n < 0) {
        throwErrno("serial write");
    }
    return static_cast<std::size_t>(n);
}

void SerialPort::flush()
{
    requireOpen();
    // Avoid mixing <termios.h> with <asm/termbits.h> in the same TU.
    constexpr int kTcIoFlush = 2;
    if (ioctl(fd_, TCFLSH, kTcIoFlush) != 0) {
        throwErrno("TCFLSH");
    }
}

const SerialPortConfig& SerialPort::config() const noexcept
{
    return config_;
}

}  // namespace lpio
