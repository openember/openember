#pragma once

#include "lpio/private/FdDeviceBase.hpp"

#include <array>
#include <cstdint>
#include <string>
#include <string_view>

namespace lpio {

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

inline constexpr std::array<SerialBaud, 8> kCommonSerialBaudRates = {
    SerialBaud::B9600,   SerialBaud::B19200,  SerialBaud::B38400,  SerialBaud::B57600,
    SerialBaud::B115200, SerialBaud::B230400, SerialBaud::B460800, SerialBaud::B921600,
};

enum class SerialParity {
    None,
    Even,
    Odd,
};

struct SerialPortConfig {
    uint32_t     baudRate   = static_cast<uint32_t>(SerialBaud::B115200);
    uint8_t      dataBits   = 8;
    uint8_t      stopBits   = 1;
    SerialParity parity     = SerialParity::None;
    bool         nonBlocking = false;
};

class SerialPort : public detail::FdDeviceBase {
public:
    class Builder {
    public:
        explicit Builder(std::string path);

        Builder& baudRate(uint32_t rate);
        Builder& baud(SerialBaud rate);
        Builder& dataBits(uint8_t bits);
        Builder& stopBits(uint8_t bits);
        Builder& parity(SerialParity p);
        Builder& nonBlocking(bool enable = true);

        SerialPort build();
        SerialPort open(OpenMode mode = OpenMode::ReadWrite);

    private:
        std::string       path_;
        SerialPortConfig  cfg_;
    };

    static Builder on(std::string path);

    SerialPort(std::string path, SerialPortConfig config);
    ~SerialPort() override;

    SerialPort(const SerialPort&)            = delete;
    SerialPort& operator=(const SerialPort&) = delete;
    SerialPort(SerialPort&& other) noexcept;
    SerialPort& operator=(SerialPort&& other) noexcept;

    virtual void open(OpenMode mode = OpenMode::ReadWrite) override;
    void close() noexcept override;

    DeviceState      state() const noexcept override;
    std::string_view path() const noexcept override;

    std::size_t read(std::span<std::byte> buf) override;
    std::size_t write(std::span<const std::byte> buf) override;
    void        flush() override;

    const SerialPortConfig& config() const noexcept;

private:
    std::string       path_;
    SerialPortConfig  config_;
    DeviceState       state_ = DeviceState::Closed;
};

}  // namespace lpio
