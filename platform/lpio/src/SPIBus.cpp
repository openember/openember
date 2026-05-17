#include "lpio/SPIBus.hpp"

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <linux/spi/spidev.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <vector>

namespace lpio {

namespace {

[[noreturn]] void throwErrno(std::string_view context)
{
    throw DeviceError(errno, context);
}

}  // namespace

SPIBus::Builder::Builder(std::string devPath) : devPath_(std::move(devPath)) {}

SPIBus::Builder& SPIBus::Builder::speedHz(uint32_t hz)
{
    cfg_.speedHz = hz;
    return *this;
}

SPIBus::Builder& SPIBus::Builder::mode(uint8_t m)
{
    cfg_.mode = m;
    return *this;
}

SPIBus::Builder& SPIBus::Builder::bitsPerWord(uint8_t bits)
{
    cfg_.bitsPerWord = bits;
    return *this;
}

SPIBus::Builder& SPIBus::Builder::delayUsecs(uint16_t us)
{
    cfg_.delayUsecs = us;
    return *this;
}

SPIBus SPIBus::Builder::build()
{
    return SPIBus(devPath_, cfg_);
}

SPIBus SPIBus::Builder::buildAndOpen(OpenMode mode)
{
    auto dev = build();
    dev.open(mode);
    return dev;
}

SPIBus::Builder SPIBus::on(std::string devPath)
{
    return Builder(std::move(devPath));
}

SPIBus::SPIBus(std::string devPath, SPIBusConfig config)
    : devPath_(std::move(devPath))
    , config_(config)
{}

SPIBus::~SPIBus()
{
    close();
}

SPIBus::SPIBus(SPIBus&& other) noexcept
    : devPath_(std::move(other.devPath_))
    , config_(other.config_)
    , state_(other.state_)
    , fd_(other.fd_)
{
    other.state_ = DeviceState::Closed;
    other.fd_    = -1;
}

SPIBus& SPIBus::operator=(SPIBus&& other) noexcept
{
    if (this != &other) {
        close();
        devPath_ = std::move(other.devPath_);
        config_  = other.config_;
        state_   = other.state_;
        fd_      = other.fd_;
        other.state_ = DeviceState::Closed;
        other.fd_    = -1;
    }
    return *this;
}

void SPIBus::applySpiSettings()
{
    if (ioctl(fd_, SPI_IOC_WR_MODE, &config_.mode) < 0) {
        throwErrno("SPI_IOC_WR_MODE");
    }
    if (ioctl(fd_, SPI_IOC_WR_BITS_PER_WORD, &config_.bitsPerWord) < 0) {
        throwErrno("SPI_IOC_WR_BITS_PER_WORD");
    }
    if (ioctl(fd_, SPI_IOC_WR_MAX_SPEED_HZ, &config_.speedHz) < 0) {
        throwErrno("SPI_IOC_WR_MAX_SPEED_HZ");
    }
}

void SPIBus::open(OpenMode mode)
{
    (void)mode;
    if (state_ == DeviceState::Open) {
        return;
    }

    if (devPath_.empty()) {
        throw DeviceError(std::errc::invalid_argument, "spi path is empty");
    }

    const int fd = ::open(devPath_.c_str(), O_RDWR);
    if (fd < 0) {
        throwErrno(devPath_);
    }

    fd_ = fd;
    try {
        applySpiSettings();
    } catch (...) {
        ::close(fd_);
        fd_ = -1;
        throw;
    }

    state_ = DeviceState::Open;
}

void SPIBus::close() noexcept
{
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
    state_ = DeviceState::Closed;
}

DeviceState SPIBus::state() const noexcept
{
    return state_;
}

std::string_view SPIBus::path() const noexcept
{
    return devPath_;
}

std::size_t SPIBus::transfer(const void* tx, void* rx, std::size_t len)
{
    requireOpen();
    if (len == 0) {
        return 0;
    }

    spi_ioc_transfer xfer {};
    xfer.tx_buf        = reinterpret_cast<unsigned long>(tx);
    xfer.rx_buf        = reinterpret_cast<unsigned long>(rx);
    xfer.len           = static_cast<uint32_t>(len);
    xfer.speed_hz      = config_.speedHz;
    xfer.delay_usecs   = config_.delayUsecs;
    xfer.bits_per_word = config_.bitsPerWord;

    if (ioctl(fd_, SPI_IOC_MESSAGE(1), &xfer) < 0) {
        throwErrno("SPI_IOC_MESSAGE");
    }
    return len;
}

std::size_t SPIBus::read(std::span<std::byte> buf)
{
    std::vector<std::byte> tx(buf.size());
    return transfer(tx.data(), buf.data(), buf.size());
}

std::size_t SPIBus::write(std::span<const std::byte> buf)
{
    std::vector<std::byte> dummy(buf.size());
    return transfer(buf.data(), dummy.data(), buf.size());
}

const SPIBusConfig& SPIBus::config() const noexcept
{
    return config_;
}

}  // namespace lpio
