#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

#include "openember/osal/types.hpp"

namespace openember::hal {

using Result = osal::Result;

struct SpiConfig {
    uint32_t speed_hz = 0;
    uint8_t mode = 0;
    uint8_t bits_per_word = 8;
    uint16_t delay_usecs = 0;
};

struct SpiCaps {
    uint32_t supports_full_duplex = 0;
    uint32_t supports_mode_count = 0;
    uint32_t min_bits_per_word = 0;
    uint32_t max_bits_per_word = 0;
};

class Spi {
public:
    Spi();
    ~Spi();

    Spi(const Spi&) = delete;
    Spi& operator=(const Spi&) = delete;
    Spi(Spi&&) = delete;
    Spi& operator=(Spi&&) = delete;

    static Result query_caps(SpiCaps* out_caps);

    Result open(const std::string& dev_path, const SpiConfig& cfg);
    Result close();
    Result transfer(const void* tx, void* rx, size_t len, size_t* out_transferred = nullptr);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace openember::hal
