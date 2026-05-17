#include "lpio/I2CBus.hpp"

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <linux/i2c-dev.h>
#include <linux/i2c.h>
#include <sys/ioctl.h>
#include <unistd.h>

namespace lpio {

namespace {

[[noreturn]] void throwErrno(std::string_view context)
{
    throw DeviceError(errno, context);
}

}  // namespace

I2CBus::Builder& I2CBus::Builder::bus(uint32_t busNum)
{
    cfg_.busNum = busNum;
    return *this;
}

I2CBus::Builder& I2CBus::Builder::address(uint8_t addr7bit)
{
    cfg_.addr7bit = addr7bit;
    return *this;
}

I2CBus I2CBus::Builder::build()
{
    return I2CBus(cfg_);
}

I2CBus I2CBus::Builder::buildAndOpen(OpenMode mode)
{
    auto dev = build();
    dev.open(mode);
    return dev;
}

I2CBus::Builder I2CBus::create()
{
    return Builder {};
}

I2CBus::I2CBus(I2CBusConfig config) : config_(config)
{
    refreshDevPath();
}

I2CBus::~I2CBus()
{
    close();
}

I2CBus::I2CBus(I2CBus&& other) noexcept
    : config_(other.config_)
    , devPath_(std::move(other.devPath_))
    , state_(other.state_)
    , fd_(other.fd_)
{
    other.state_ = DeviceState::Closed;
    other.fd_    = -1;
}

I2CBus& I2CBus::operator=(I2CBus&& other) noexcept
{
    if (this != &other) {
        close();
        config_  = other.config_;
        devPath_ = std::move(other.devPath_);
        state_   = other.state_;
        fd_      = other.fd_;
        other.state_ = DeviceState::Closed;
        other.fd_    = -1;
    }
    return *this;
}

void I2CBus::refreshDevPath()
{
    char path[64];
    std::snprintf(path, sizeof(path), "/dev/i2c-%u", config_.busNum);
    devPath_ = path;
}

void I2CBus::open(OpenMode mode)
{
    (void)mode;
    if (state_ == DeviceState::Open) {
        return;
    }

    refreshDevPath();

    const int fd = ::open(devPath_.c_str(), O_RDWR);
    if (fd < 0) {
        throwErrno(devPath_);
    }

    if (ioctl(fd, I2C_SLAVE, static_cast<int>(config_.addr7bit)) < 0) {
        ::close(fd);
        throwErrno("I2C_SLAVE");
    }

    fd_    = fd;
    state_ = DeviceState::Open;
}

void I2CBus::close() noexcept
{
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
    state_ = DeviceState::Closed;
}

DeviceState I2CBus::state() const noexcept
{
    return state_;
}

std::string_view I2CBus::path() const noexcept
{
    return devPath_;
}

std::size_t I2CBus::read(std::span<std::byte> buf)
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
        throwErrno("i2c read");
    }
    return static_cast<std::size_t>(n);
}

std::size_t I2CBus::write(std::span<const std::byte> buf)
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
        throwErrno("i2c write");
    }
    return static_cast<std::size_t>(n);
}

std::size_t I2CBus::writeRead(const uint8_t* wbuf, std::size_t wlen, uint8_t* rbuf, std::size_t rlen)
{
    requireOpen();
    if (wlen > 0 && !wbuf) {
        throw DeviceError(std::errc::invalid_argument, "writeRead wbuf");
    }
    if (rlen > 0 && !rbuf) {
        throw DeviceError(std::errc::invalid_argument, "writeRead rbuf");
    }

    if (wlen == 0) {
        return read(std::span<std::byte>(reinterpret_cast<std::byte*>(rbuf), rlen));
    }
    if (rlen == 0) {
        return write(std::span<const std::byte>(reinterpret_cast<const std::byte*>(wbuf), wlen));
    }

    i2c_msg msgs[2] {};
    msgs[0].addr  = config_.addr7bit;
    msgs[0].flags = 0;
    msgs[0].len   = static_cast<uint16_t>(wlen);
    msgs[0].buf   = const_cast<uint8_t*>(wbuf);

    msgs[1].addr  = config_.addr7bit;
    msgs[1].flags = I2C_M_RD;
    msgs[1].len   = static_cast<uint16_t>(rlen);
    msgs[1].buf   = rbuf;

    i2c_rdwr_ioctl_data data {};
    data.msgs  = msgs;
    data.nmsgs = 2;

    if (ioctl(fd_, I2C_RDWR, &data) < 0) {
        throwErrno("I2C_RDWR");
    }
    return rlen;
}

const I2CBusConfig& I2CBus::config() const noexcept
{
    return config_;
}

}  // namespace lpio
