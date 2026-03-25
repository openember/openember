/* OpenEmber OSAL — C++ wrapper: Shared Memory (RAII) */
#ifndef OPENEMBER_OSAL_SHM_HPP_
#define OPENEMBER_OSAL_SHM_HPP_

#include "openember/osal/shm.h"

#include <cstddef>
#include <string>

namespace oe {
namespace osal {

class Shm {
public:
    Shm() = default;
    Shm(const Shm &) = delete;
    Shm &operator=(const Shm &) = delete;

    ~Shm()
    {
        (void)oe_shm_close(&shm_);
    }

    oe_result_t create(const std::string &name, size_t size)
    {
        return oe_shm_create(&shm_, name.c_str(), size);
    }

    oe_result_t open(const std::string &name)
    {
        return oe_shm_open(&shm_, name.c_str());
    }

    oe_result_t close()
    {
        return oe_shm_close(&shm_);
    }

    oe_result_t query_caps(oe_shm_caps_t *out_caps) const
    {
        return oe_shm_query_caps(out_caps);
    }

    oe_result_t get_ptr(void **out_addr, size_t *out_size)
    {
        return oe_shm_get_ptr(&shm_, out_addr, out_size);
    }

private:
    oe_shm_t shm_ {};
};

} // namespace osal
} // namespace oe

#endif /* OPENEMBER_OSAL_SHM_HPP_ */

