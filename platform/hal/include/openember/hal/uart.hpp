/* OpenEmber HAL — C++ wrapper: UART (RAII)
 *
 * This header provides a thin C++ layer on top of the C ABI in uart.h.
 */
#ifndef OPENEMBER_HAL_UART_HPP_
#define OPENEMBER_HAL_UART_HPP_

#include "openember/hal/uart.h"

#include <string>

namespace oe {
namespace hal {

class UART {
public:
    UART() = default;

    UART(const std::string &path, const oe_uart_config_t &cfg)
    {
        (void)open(path, cfg);
    }

    ~UART()
    {
        (void)oe_uart_close(&u_);
    }

    UART(const UART &) = delete;
    UART &operator=(const UART &) = delete;

    UART(UART &&other) noexcept
    {
        u_ = other.u_;
        opened_ = other.opened_;
        other.opened_ = false;
    }

    UART &operator=(UART &&other) noexcept
    {
        if (this != &other) {
            (void)oe_uart_close(&u_);
            u_ = other.u_;
            opened_ = other.opened_;
            other.opened_ = false;
        }
        return *this;
    }

    oe_result_t open(const std::string &path, const oe_uart_config_t &cfg)
    {
        oe_result_t r = oe_uart_open(&u_, path.c_str(), &cfg);
        opened_ = (r == OE_OK);
        return r;
    }

    oe_result_t close()
    {
        opened_ = false;
        return oe_uart_close(&u_);
    }

    oe_result_t read(void *buf, size_t len, size_t *out_read = nullptr)
    {
        return oe_uart_read(&u_, buf, len, out_read);
    }

    oe_result_t write(const void *buf, size_t len, size_t *out_written = nullptr)
    {
        return oe_uart_write(&u_, buf, len, out_written);
    }

    oe_result_t query_caps(oe_uart_caps_t *out_caps) const
    {
        return oe_uart_query_caps(out_caps);
    }

private:
    oe_uart_t u_ {};
    bool opened_ = false;
};

} // namespace hal
} // namespace oe

#endif /* OPENEMBER_HAL_UART_HPP_ */

