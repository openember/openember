/* OpenEmber HAL — C++ wrapper: SBUS (RAII) */
#ifndef OPENEMBER_HAL_SBUS_HPP_
#define OPENEMBER_HAL_SBUS_HPP_

#include "openember/hal/sbus.h"

#include <string>

namespace oe {
namespace hal {

class SBUS {
public:
    SBUS() = default;
    SBUS(const SBUS &) = delete;
    SBUS &operator=(const SBUS &) = delete;
    SBUS(SBUS &&) = delete;
    SBUS &operator=(SBUS &&) = delete;

    ~SBUS()
    {
        (void)oe_sbus_close(&s_);
    }

    oe_result_t open(const std::string &uart_path, const oe_sbus_config_t &cfg)
    {
        opened_ = false;
        oe_result_t r = oe_sbus_open(&s_, uart_path.c_str(), &cfg);
        opened_ = (r == OE_OK);
        return r;
    }

    oe_result_t close()
    {
        opened_ = false;
        return oe_sbus_close(&s_);
    }

    oe_result_t query_caps(oe_sbus_caps_t *out_caps) const
    {
        return oe_sbus_query_caps(out_caps);
    }

    oe_result_t recv_frame(oe_sbus_frame_t *out_frame, int timeout_ms)
    {
        return oe_sbus_recv_frame(&s_, out_frame, timeout_ms);
    }

private:
    oe_sbus_t s_ {};
    bool opened_ = false;
};

} // namespace hal
} // namespace oe

#endif /* OPENEMBER_HAL_SBUS_HPP_ */

