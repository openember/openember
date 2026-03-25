/* OpenEmber OSAL — C++ wrapper: FIFO (RAII) */
#ifndef OPENEMBER_OSAL_FIFO_HPP_
#define OPENEMBER_OSAL_FIFO_HPP_

#include "openember/osal/fifo.h"

#include <cstdint>
#include <string>

namespace oe {
namespace osal {

class Fifo {
public:
    Fifo() = default;
    Fifo(const Fifo &) = delete;
    Fifo &operator=(const Fifo &) = delete;

    ~Fifo()
    {
        (void)oe_fifo_close(&f_);
    }

    oe_result_t open(const std::string &path, uint32_t mode)
    {
        return oe_fifo_open(&f_, path.c_str(), mode);
    }

    oe_result_t close()
    {
        return oe_fifo_close(&f_);
    }

    oe_result_t write(const void *buf, size_t len, size_t *out_written, int timeout_ms)
    {
        return oe_fifo_write(&f_, buf, len, out_written, timeout_ms);
    }

    oe_result_t read(void *buf, size_t len, size_t *out_read, int timeout_ms)
    {
        return oe_fifo_read(&f_, buf, len, out_read, timeout_ms);
    }

    static oe_result_t unlink(const std::string &path)
    {
        return oe_fifo_unlink(path.c_str());
    }

private:
    oe_fifo_t f_ {};
};

} // namespace osal
} // namespace oe

#endif /* OPENEMBER_OSAL_FIFO_HPP_ */

