/* OpenEmber HAL — C++ wrapper: GPIO (RAII) */
#ifndef OPENEMBER_HAL_GPIO_HPP_
#define OPENEMBER_HAL_GPIO_HPP_

#include "openember/hal/gpio.h"

#include <cstdint>

namespace oe {
namespace hal {

class GPIO {
public:
    GPIO() = default;
    GPIO(const GPIO &) = delete;
    GPIO &operator=(const GPIO &) = delete;
    GPIO(GPIO &&) = delete;
    GPIO &operator=(GPIO &&) = delete;

    ~GPIO()
    {
        (void)oe_gpio_close(&g_);
    }

    oe_result_t open(uint32_t gpio_num)
    {
        opened_ = false;
        oe_result_t r = oe_gpio_open(&g_, gpio_num);
        opened_ = (r == OE_OK);
        return r;
    }

    oe_result_t close()
    {
        opened_ = false;
        return oe_gpio_close(&g_);
    }

    oe_result_t set_direction(oe_gpio_direction_t dir)
    {
        return oe_gpio_set_direction(&g_, dir);
    }

    oe_result_t read(uint8_t *out_value)
    {
        return oe_gpio_read(&g_, out_value);
    }

    oe_result_t write(uint8_t value)
    {
        return oe_gpio_write(&g_, value);
    }

    oe_result_t query_caps(oe_gpio_caps_t *out_caps) const
    {
        return oe_gpio_query_caps(out_caps);
    }

private:
    oe_gpio_t g_ {};
    bool opened_ = false;
};

} // namespace hal
} // namespace oe

#endif /* OPENEMBER_HAL_GPIO_HPP_ */

