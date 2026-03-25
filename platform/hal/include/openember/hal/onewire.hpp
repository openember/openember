/* OpenEmber HAL — C++ wrapper: 1-Wire (RAII) */
#ifndef OPENEMBER_HAL_ONEWIRE_HPP_
#define OPENEMBER_HAL_ONEWIRE_HPP_

#include "openember/hal/onewire.h"

#include <string>

namespace oe {
namespace hal {

class OneWire {
public:
    OneWire() = default;
    OneWire(const OneWire &) = delete;
    OneWire &operator=(const OneWire &) = delete;
    OneWire(OneWire &&) = delete;
    OneWire &operator=(OneWire &&) = delete;

    ~OneWire()
    {
        (void)oe_onewire_close(&w_);
    }

    oe_result_t open(const std::string &w1_slave_path)
    {
        return oe_onewire_open(&w_, w1_slave_path.c_str());
    }

    oe_result_t close()
    {
        return oe_onewire_close(&w_);
    }

    oe_result_t query_caps(oe_onewire_caps_t *out_caps) const
    {
        return oe_onewire_query_caps(out_caps);
    }

    oe_result_t read_temperature(oe_onewire_temp_t *out_temp)
    {
        return oe_onewire_read_temperature(&w_, out_temp);
    }

    oe_result_t read_raw(char *out_buf, size_t cap, size_t *out_len = nullptr)
    {
        return oe_onewire_read_raw(&w_, out_buf, cap, out_len);
    }

private:
    oe_onewire_t w_ {};
};

} // namespace hal
} // namespace oe

#endif /* OPENEMBER_HAL_ONEWIRE_HPP_ */

