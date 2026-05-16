#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

#include "openember/osal/types.hpp"

namespace openember::hal {

using Result = osal::Result;

inline constexpr uint32_t kUartCapsMaxBaudRates = 8;

enum class UartParity {
    None = 0,
    Even = 1,
    Odd = 2,
};

enum class UartParityMask : uint32_t {
    None = 1u << 0,
    Even = 1u << 1,
    Odd = 1u << 2,
};

struct UartCaps {
    uint32_t baud_rate_count = 0;
    uint32_t baud_rates[kUartCapsMaxBaudRates] {};
    uint32_t parity_mask = 0;
    uint32_t data_bits_min = 0;
    uint32_t data_bits_max = 0;
    uint32_t stop_bits_min = 0;
    uint32_t stop_bits_max = 0;
};

struct UartConfig {
    uint32_t baud_rate = 115200;
    uint8_t data_bits = 8;
    uint8_t stop_bits = 1;
    UartParity parity = UartParity::None;
    uint8_t nonblocking = 0;
};

class Uart {
public:
    Uart();
    ~Uart();

    Uart(const Uart&) = delete;
    Uart& operator=(const Uart&) = delete;
    Uart(Uart&&) = delete;
    Uart& operator=(Uart&&) = delete;

    static Result query_caps(UartCaps* out_caps);

    Result open(const std::string& path, const UartConfig& cfg);
    Result close();
    Result read(void* buf, size_t len, size_t* out_read = nullptr);
    Result write(const void* buf, size_t len, size_t* out_written = nullptr);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace openember::hal
