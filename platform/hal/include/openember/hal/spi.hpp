/* OpenEmber HAL — C++ wrapper: SPI (RAII) */
#ifndef OPENEMBER_HAL_SPI_HPP_
#define OPENEMBER_HAL_SPI_HPP_

#include "openember/hal/spi.h"

namespace oe {
namespace hal {

class SPI {
public:
    SPI() = default;
    SPI(const SPI &) = delete;
    SPI &operator=(const SPI &) = delete;
    SPI(SPI &&) = delete;
    SPI &operator=(SPI &&) = delete;

    ~SPI()
    {
        (void)oe_spi_close(&s_);
    }

    oe_result_t open(const char *dev_path, const oe_spi_config_t &cfg)
    {
        opened_ = false;
        oe_result_t r = oe_spi_open(&s_, dev_path, &cfg);
        opened_ = (r == OE_OK);
        return r;
    }

    oe_result_t close()
    {
        opened_ = false;
        return oe_spi_close(&s_);
    }

    oe_result_t query_caps(oe_spi_caps_t *out_caps) const
    {
        return oe_spi_query_caps(out_caps);
    }

    oe_result_t transfer(const void *tx, void *rx, size_t len, size_t *out_transferred = nullptr)
    {
        return oe_spi_transfer(&s_, tx, rx, len, out_transferred);
    }

private:
    oe_spi_t s_ {};
    bool opened_ = false;
};

} // namespace hal
} // namespace oe

#endif /* OPENEMBER_HAL_SPI_HPP_ */

