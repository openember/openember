#define _GNU_SOURCE

#include "openember/hal/spi.hpp"

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <linux/spi/spidev.h>
#include <sys/ioctl.h>
#include <unistd.h>

namespace openember::hal {

struct Spi::Impl {
    int fd = -1;
    bool inited = false;
    SpiConfig cfg {};
};

Spi::Spi() : impl_(std::make_unique<Impl>()) {}

Spi::~Spi()
{
    (void)close();
}

Result Spi::query_caps(SpiCaps* out_caps)
{
    if (!out_caps) {
        return osal::kErrInvalidArg;
    }

    out_caps->supports_full_duplex = 1;
    out_caps->supports_mode_count = 4;
    out_caps->min_bits_per_word = 8;
    out_caps->max_bits_per_word = 32;
    return osal::kOk;
}

Result Spi::open(const std::string& dev_path, const SpiConfig& cfg)
{
    if (dev_path.empty()) {
        return osal::kErrInvalidArg;
    }

    if (cfg.bits_per_word == 0) {
        return osal::kErrInvalidArg;
    }

    (void)close();

    const int fd = ::open(dev_path.c_str(), O_RDWR);
    if (fd < 0) {
        return osal::kErrInternal;
    }

    if (ioctl(fd, SPI_IOC_WR_MODE, &cfg.mode) < 0) {
        ::close(fd);
        return osal::kErrUnsupported;
    }
    if (ioctl(fd, SPI_IOC_WR_BITS_PER_WORD, &cfg.bits_per_word) < 0) {
        ::close(fd);
        return osal::kErrUnsupported;
    }
    if (ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &cfg.speed_hz) < 0) {
        ::close(fd);
        return osal::kErrUnsupported;
    }

    impl_->fd = fd;
    impl_->inited = true;
    impl_->cfg = cfg;
    return osal::kOk;
}

Result Spi::close()
{
    const int fd = impl_->fd;
    impl_->fd = -1;
    impl_->inited = false;
    impl_->cfg = {};

    if (fd >= 0) {
        return ::close(fd) == 0 ? osal::kOk : osal::kErrInternal;
    }
    return osal::kOk;
}

Result Spi::transfer(const void* tx, void* rx, size_t len, size_t* out_transferred)
{
    if (len == 0) {
        if (out_transferred) {
            *out_transferred = 0;
        }
        return osal::kOk;
    }

    if (!impl_->inited || impl_->fd < 0) {
        return osal::kErrInvalidArg;
    }

    const uint8_t* txp = static_cast<const uint8_t*>(tx);
    uint8_t* rxp = static_cast<uint8_t*>(rx);
    uint8_t* zero_tx = nullptr;

    if (!txp) {
        zero_tx = static_cast<uint8_t*>(std::calloc(len, 1));
        if (!zero_tx) {
            return osal::kErrNoMem;
        }
        txp = zero_tx;
    }

    spi_ioc_transfer tr {};
    tr.tx_buf = reinterpret_cast<uintptr_t>(txp);
    tr.rx_buf = reinterpret_cast<uintptr_t>(rxp);
    tr.len = static_cast<uint32_t>(len);
    tr.delay_usecs = impl_->cfg.delay_usecs;
    tr.speed_hz = impl_->cfg.speed_hz;
    tr.bits_per_word = impl_->cfg.bits_per_word;

    const int rc = ioctl(impl_->fd, SPI_IOC_MESSAGE(1), &tr);
    std::free(zero_tx);

    if (rc < 0) {
        return osal::kErrInternal;
    }

    if (out_transferred) {
        *out_transferred = len;
    }
    return osal::kOk;
}

}  // namespace openember::hal
