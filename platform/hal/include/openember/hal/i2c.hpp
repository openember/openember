/* OpenEmber HAL — C++ wrapper: I2C (RAII) */
#ifndef OPENEMBER_HAL_I2C_HPP_
#define OPENEMBER_HAL_I2C_HPP_

#include "openember/hal/i2c.h"

#include <cstddef>
#include <cstdint>

namespace oe {
namespace hal {

class I2C {
public:
    I2C() = default;
    I2C(const I2C &) = delete;
    I2C &operator=(const I2C &) = delete;
    I2C(I2C &&) = delete;
    I2C &operator=(I2C &&) = delete;

    ~I2C()
    {
        (void)oe_i2c_close(&i_);
    }

    oe_result_t open(uint32_t bus_num, uint8_t addr_7bit)
    {
        opened_ = false;
        oe_result_t r = oe_i2c_open(&i_, bus_num, addr_7bit);
        opened_ = (r == OE_OK);
        return r;
    }

    oe_result_t close()
    {
        opened_ = false;
        return oe_i2c_close(&i_);
    }

    oe_result_t write(const uint8_t *data, size_t len, size_t *out_written = nullptr)
    {
        return oe_i2c_write(&i_, data, len, out_written);
    }

    oe_result_t read(uint8_t *data, size_t len, size_t *out_read = nullptr)
    {
        return oe_i2c_read(&i_, data, len, out_read);
    }

    oe_result_t write_read(const uint8_t *wbuf, size_t wlen, uint8_t *rbuf, size_t rlen)
    {
        return oe_i2c_write_read(&i_, wbuf, wlen, rbuf, rlen);
    }

    oe_result_t query_caps(oe_i2c_caps_t *out_caps) const
    {
        return oe_i2c_query_caps(out_caps);
    }

private:
    oe_i2c_t i_ {};
    bool opened_ = false;
};

} // namespace hal
} // namespace oe

#endif /* OPENEMBER_HAL_I2C_HPP_ */

