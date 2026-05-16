#pragma once

#include <cstdint>
#include <memory>

#include "openember/osal/types.hpp"

namespace openember::hal {

using Result = osal::Result;

enum class GpioDirection {
    In = 0,
    Out = 1,
};

enum class GpioCapsFlag : uint32_t {
    DirIn = 1u << 0,
    DirOut = 1u << 1,
};

struct GpioCaps {
    uint32_t direction_mask = 0;
};

class Gpio {
public:
    Gpio();
    ~Gpio();

    Gpio(const Gpio&) = delete;
    Gpio& operator=(const Gpio&) = delete;
    Gpio(Gpio&&) = delete;
    Gpio& operator=(Gpio&&) = delete;

    static Result query_caps(GpioCaps* out_caps);

    Result open(uint32_t gpio_num);
    Result set_direction(GpioDirection dir);
    Result read(uint8_t* out_value);
    Result write(uint8_t value);
    Result close();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace openember::hal
