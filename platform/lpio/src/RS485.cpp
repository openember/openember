#include "lpio/RS485.hpp"

#include <cerrno>

#ifdef __linux__
#include <linux/serial.h>
#include <sys/ioctl.h>
#endif

namespace lpio {

namespace {

[[noreturn]] void throwErrno(std::string_view context)
{
    throw DeviceError(errno, context);
}

}  // namespace

RS485::Builder::Builder(std::string path) : path_(std::move(path)) {}

RS485::Builder& RS485::Builder::baudRate(uint32_t rate)
{
    serialCfg_.baudRate = rate;
    return *this;
}

RS485::Builder& RS485::Builder::baud(SerialBaud rate)
{
    serialCfg_.baudRate = static_cast<uint32_t>(rate);
    return *this;
}

RS485::Builder& RS485::Builder::dataBits(uint8_t bits)
{
    serialCfg_.dataBits = bits;
    return *this;
}

RS485::Builder& RS485::Builder::stopBits(uint8_t bits)
{
    serialCfg_.stopBits = bits;
    return *this;
}

RS485::Builder& RS485::Builder::parity(SerialParity p)
{
    serialCfg_.parity = p;
    return *this;
}

RS485::Builder& RS485::Builder::nonBlocking(bool enable)
{
    serialCfg_.nonBlocking = enable;
    return *this;
}

RS485::Builder& RS485::Builder::rs485(RS485Config cfg)
{
    rs485Cfg_ = cfg;
    return *this;
}

RS485 RS485::Builder::build()
{
    return RS485(path_, serialCfg_, rs485Cfg_);
}

RS485 RS485::Builder::open(OpenMode mode)
{
    auto dev = build();
    dev.open(mode);
    return dev;
}

RS485::Builder RS485::on(std::string path)
{
    return Builder(std::move(path));
}

RS485::RS485(std::string path, SerialPortConfig serialCfg, RS485Config rs485Cfg)
    : SerialPort(std::move(path), serialCfg)
    , rs485Cfg_(rs485Cfg)
{}

RS485::RS485(RS485&& other) noexcept
    : SerialPort(std::move(other))
    , rs485Cfg_(other.rs485Cfg_)
{}

RS485& RS485::operator=(RS485&& other) noexcept
{
    if (this != &other) {
        SerialPort::operator=(std::move(other));
        rs485Cfg_ = other.rs485Cfg_;
    }
    return *this;
}

void RS485::open(OpenMode mode)
{
    const bool wasOpen = isOpen();
    SerialPort::open(mode);
    try {
        applyRs485Settings();
    } catch (...) {
        if (!wasOpen) {
            SerialPort::close();
        }
        throw;
    }
}

void RS485::applyRs485Settings()
{
#ifdef __linux__
    if (rs485Cfg_.mode == RS485Mode::Disabled) {
        return;
    }

    requireOpen();

    serial_rs485 rs {};
    rs.flags |= SER_RS485_ENABLED;
    if (rs485Cfg_.rtsOnSend) {
        rs.flags |= SER_RS485_RTS_ON_SEND;
    }
    if (rs485Cfg_.rtsAfterSend) {
        rs.flags |= SER_RS485_RTS_AFTER_SEND;
    }
    rs.delay_rts_before_send = rs485Cfg_.delayRtsBeforeSendUs;
    rs.delay_rts_after_send  = rs485Cfg_.delayRtsAfterSendUs;

    if (ioctl(fd(), TIOCSRS485, &rs) != 0) {
        throwErrno("TIOCSRS485");
    }
#else
    throw DeviceError(std::errc::not_supported, "RS485 requires Linux");
#endif
}

const RS485Config& RS485::rs485Config() const noexcept
{
    return rs485Cfg_;
}

}  // namespace lpio
