/* OpenEmber OSAL — C++ wrapper: Pipe (RAII) */
#ifndef OPENEMBER_OSAL_PIPE_HPP_
#define OPENEMBER_OSAL_PIPE_HPP_

#include "openember/osal/pipe.h"

#include <cstddef>

namespace oe {
namespace osal {

class Pipe {
public:
    Pipe() = default;
    Pipe(const Pipe &) = delete;
    Pipe &operator=(const Pipe &) = delete;

    ~Pipe()
    {
        (void)oe_pipe_close(&p_);
    }

    oe_result_t create()
    {
        return oe_pipe_create(&p_);
    }

    oe_result_t close()
    {
        return oe_pipe_close(&p_);
    }

    oe_result_t write(const void *buf, size_t len, size_t *out_written, int timeout_ms)
    {
        return oe_pipe_write(&p_, buf, len, out_written, timeout_ms);
    }

    oe_result_t read(void *buf, size_t len, size_t *out_read, int timeout_ms)
    {
        return oe_pipe_read(&p_, buf, len, out_read, timeout_ms);
    }

private:
    oe_pipe_t p_ {};
};

} // namespace osal
} // namespace oe

#endif /* OPENEMBER_OSAL_PIPE_HPP_ */

