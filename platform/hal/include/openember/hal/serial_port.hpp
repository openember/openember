#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

#include "openember/osal/types.hpp"

namespace openember::hal {

using Result = osal::Result;

inline constexpr uint32_t kSerialPortCapsMaxBaudRates = 8;

/** 常用波特率（可直接用于 SerialPortConfig::baud_rate） */
enum class SerialBaud : uint32_t {
    B9600 = 9600,
    B19200 = 19200,
    B38400 = 38400,
    B57600 = 57600,
    B115200 = 115200,
    B230400 = 230400,
    B460800 = 460800,
    B921600 = 921600,
};

inline constexpr std::array<SerialBaud, kSerialPortCapsMaxBaudRates> kCommonSerialBaudRates = {
    SerialBaud::B9600,   SerialBaud::B19200,  SerialBaud::B38400,  SerialBaud::B57600,
    SerialBaud::B115200, SerialBaud::B230400, SerialBaud::B460800, SerialBaud::B921600,
};

/** 是否为驱动预置的标准波特率（可用 termios 常量配置） */
bool is_standard_serial_baud(uint32_t baud_rate);

enum class SerialParity {
    None = 0,
    Even = 1,
    Odd = 2,
};

enum class SerialParityMask : uint32_t {
    None = 1u << 0,
    Even = 1u << 1,
    Odd = 1u << 2,
};

struct SerialPortCaps {
    uint32_t baud_rate_count = 0;
    uint32_t baud_rates[kSerialPortCapsMaxBaudRates] {};
    uint32_t parity_mask = 0;
    uint32_t data_bits_min = 0;
    uint32_t data_bits_max = 0;
    uint32_t stop_bits_min = 0;
    uint32_t stop_bits_max = 0;
};

struct SerialPortConfig {
    /** 任意正整数波特率；非标准值在 Linux 上通过 termios2(BOTHER) 配置 */
    uint32_t baud_rate = static_cast<uint32_t>(SerialBaud::B115200);
    uint8_t data_bits = 8;
    uint8_t stop_bits = 1;
    SerialParity parity = SerialParity::None;
    /** 1：保持 O_NONBLOCK（如 SBUS 轮询超时） */
    uint8_t nonblocking = 0;

    static SerialPortConfig with_baud(SerialBaud baud)
    {
        SerialPortConfig cfg;
        cfg.baud_rate = static_cast<uint32_t>(baud);
        return cfg;
    }
};

class SerialPort {
public:
    SerialPort();
    virtual ~SerialPort();

    SerialPort(const SerialPort&) = delete;
    SerialPort& operator=(const SerialPort&) = delete;
    SerialPort(SerialPort&&) = delete;
    SerialPort& operator=(SerialPort&&) = delete;

    static Result query_caps(SerialPortCaps* out_caps);

    Result open(const std::string& path, const SerialPortConfig& cfg);
    Result close();
    Result read(void* buf, size_t len, size_t* out_read = nullptr);
    Result write(const void* buf, size_t len, size_t* out_written = nullptr);

    bool is_open() const;

protected:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace openember::hal
