#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

#include "openember/osal/types.hpp"

namespace openember::hal {

using Result = osal::Result;

struct OnewireCaps {
    uint32_t supports_temperature = 0;
    uint32_t supports_raw_read = 0;
};

struct OnewireTemp {
    uint8_t crc_ok = 0;
    int32_t temperature_milli_c = 0;
    float temperature_c = 0.0f;
};

class Onewire {
public:
    Onewire();
    ~Onewire();

    Onewire(const Onewire&) = delete;
    Onewire& operator=(const Onewire&) = delete;
    Onewire(Onewire&&) = delete;
    Onewire& operator=(Onewire&&) = delete;

    static Result query_caps(OnewireCaps* out_caps);

    Result open(const std::string& w1_slave_path);
    Result close();
    Result read_temperature(OnewireTemp* out_temp);
    Result read_raw(char* out_buf, size_t cap, size_t* out_len = nullptr);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace openember::hal
