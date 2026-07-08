#pragma once

#include "lpio/SerialPort.hpp"

#include <cstdint>

namespace lpio {

enum class RS485Mode {
    Disabled,
    Enabled,
};

struct RS485Config {
    RS485Mode mode              = RS485Mode::Enabled;
    bool      rtsOnSend         = false;
    bool      rtsAfterSend      = false;
    uint32_t  delayRtsBeforeSendUs = 0;
    uint32_t  delayRtsAfterSendUs  = 0;
};

class RS485 final : public SerialPort {
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
        Builder& rs485(RS485Config cfg);

        RS485 build();
        RS485 open(OpenMode mode = OpenMode::ReadWrite);

    private:
        std::string       path_;
        SerialPortConfig  serialCfg_;
        RS485Config       rs485Cfg_;
    };

    static Builder on(std::string path);

    RS485(std::string path, SerialPortConfig serialCfg, RS485Config rs485Cfg);
    ~RS485() override = default;

    RS485(const RS485&)            = delete;
    RS485& operator=(const RS485&) = delete;
    RS485(RS485&& other) noexcept;
    RS485& operator=(RS485&& other) noexcept;

    void open(OpenMode mode = OpenMode::ReadWrite) override;

    const RS485Config& rs485Config() const noexcept;

private:
    void applyRs485Settings();

    RS485Config rs485Cfg_;
};

}  // namespace lpio
