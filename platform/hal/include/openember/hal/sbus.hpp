#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

#include "openember/hal/serial_port.hpp"

namespace openember::hal {

struct SbusCaps {
    uint32_t channels_count = 0;
    uint32_t switches_count = 0;
    uint32_t frame_len_bytes = 0;
    uint32_t baud_rate = 0;
};

struct SbusFrame {
    uint16_t channels[16] {};
    uint8_t switches[2] {};
    uint8_t frame_lost = 0;
    uint8_t failsafe = 0;
    uint8_t raw[25] {};
};

struct SbusConfig {
    /** 0 表示默认 100000（SBUS 标准波特率，非 termios 预置常量） */
    uint32_t baud_rate = 0;
    uint8_t nonblocking = 0;
};

class Sbus : public SerialPort {
public:
    Sbus() = default;
    ~Sbus() override = default;

    Sbus(const Sbus&) = delete;
    Sbus& operator=(const Sbus&) = delete;
    Sbus(Sbus&&) = delete;
    Sbus& operator=(Sbus&&) = delete;

    static Result query_caps(SbusCaps* out_caps);

    Result open(const std::string& uart_path, const SbusConfig& cfg = {});
    Result recv_frame(SbusFrame* out_frame, int timeout_ms);
};

}  // namespace openember::hal
