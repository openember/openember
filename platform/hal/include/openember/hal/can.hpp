/* OpenEmber HAL — C++ wrapper: CAN (RAII) */
#ifndef OPENEMBER_HAL_CAN_HPP_
#define OPENEMBER_HAL_CAN_HPP_

#include "openember/hal/can.h"

#include <string>

namespace oe {
namespace hal {

class CAN {
public:
    CAN() = default;
    CAN(const CAN &) = delete;
    CAN &operator=(const CAN &) = delete;
    CAN(CAN &&) = delete;
    CAN &operator=(CAN &&) = delete;

    ~CAN()
    {
        (void)oe_can_close(&c_);
    }

    oe_result_t open(const std::string &ifname, const oe_can_config_t &cfg)
    {
        opened_ = false;
        oe_result_t r = oe_can_open(&c_, ifname.c_str(), &cfg);
        opened_ = (r == OE_OK);
        return r;
    }

    oe_result_t close()
    {
        opened_ = false;
        return oe_can_close(&c_);
    }

    oe_result_t send(const oe_can_frame_t &frame)
    {
        return oe_can_send_frame(&c_, &frame);
    }

    oe_result_t recv(oe_can_frame_t *out_frame, int timeout_ms)
    {
        return oe_can_recv_frame(&c_, out_frame, timeout_ms);
    }

    oe_result_t query_caps(oe_can_caps_t *out_caps) const
    {
        return oe_can_query_caps(out_caps);
    }

private:
    oe_can_t c_ {};
    bool opened_ = false;
};

} // namespace hal
} // namespace oe

#endif /* OPENEMBER_HAL_CAN_HPP_ */

