#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>

#include "openember/osal/types.hpp"

namespace openember::hal {

using Result = osal::Result;

struct I2cCaps {
    uint32_t max_write_bytes = 0;
    uint32_t max_read_bytes = 0;
    uint32_t supports_write_read = 0;
};

class I2c {
public:
    I2c();
    ~I2c();

    I2c(const I2c&) = delete;
    I2c& operator=(const I2c&) = delete;
    I2c(I2c&&) = delete;
    I2c& operator=(I2c&&) = delete;

    static Result query_caps(I2cCaps* out_caps);

    Result open(uint32_t bus_num, uint8_t addr_7bit);
    Result close();
    Result write(const uint8_t* data, size_t len, size_t* out_written = nullptr);
    Result read(uint8_t* data, size_t len, size_t* out_read = nullptr);
    Result write_read(const uint8_t* wbuf, size_t wlen, uint8_t* rbuf, size_t rlen);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace openember::hal
