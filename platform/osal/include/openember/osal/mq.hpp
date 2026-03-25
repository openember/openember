/* OpenEmber OSAL — C++ wrapper: Message Queue (RAII) */
#ifndef OPENEMBER_OSAL_MQ_HPP_
#define OPENEMBER_OSAL_MQ_HPP_

#include "openember/osal/mq.h"

#include <cstddef>
#include <cstdint>
#include <string>

namespace oe {
namespace osal {

class Mq {
public:
    Mq() = default;
    Mq(const Mq &) = delete;
    Mq &operator=(const Mq &) = delete;

    ~Mq()
    {
        (void)oe_mq_close(&mq_);
    }

    oe_result_t create(const std::string &name, uint32_t max_msgs, uint32_t msg_size)
    {
        return oe_mq_create(&mq_, name.c_str(), max_msgs, msg_size);
    }

    oe_result_t open(const std::string &name)
    {
        return oe_mq_open(&mq_, name.c_str());
    }

    oe_result_t close()
    {
        return oe_mq_close(&mq_);
    }

    static oe_result_t unlink(const std::string &name)
    {
        return oe_mq_unlink(name.c_str());
    }

    oe_result_t query_caps(oe_mq_caps_t *out_caps) const
    {
        return oe_mq_query_caps(out_caps);
    }

    oe_result_t send(const void *buf, size_t len, int timeout_ms)
    {
        return oe_mq_send(&mq_, buf, len, timeout_ms);
    }

    oe_result_t recv(void *buf, size_t cap, size_t *out_len, int timeout_ms)
    {
        return oe_mq_recv(&mq_, buf, cap, out_len, timeout_ms);
    }

private:
    oe_mq_t mq_ {};
};

} // namespace osal
} // namespace oe

#endif /* OPENEMBER_OSAL_MQ_HPP_ */

