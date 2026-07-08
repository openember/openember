#include "lpio/PWM.hpp"

#include <cerrno>
#include <fstream>
#include <sstream>

namespace lpio {

namespace {

[[noreturn]] void throwErrno(std::string_view context)
{
    throw DeviceError(errno, context);
}

void writeFile(const std::string& path, std::string_view value)
{
    std::ofstream out(path);
    if (!out) {
        throw DeviceError(std::errc::io_error, path);
    }
    out << value;
    if (!out) {
        throw DeviceError(std::errc::io_error, path);
    }
}

}  // namespace

PWM::Builder::Builder(std::string chipPath)
{
    cfg_.chipPath = std::move(chipPath);
}

PWM::Builder& PWM::Builder::channel(uint32_t ch)
{
    cfg_.channel = ch;
    return *this;
}

PWM::Builder& PWM::Builder::periodNs(uint64_t ns)
{
    cfg_.periodNs = ns;
    return *this;
}

PWM::Builder& PWM::Builder::dutyCycleNs(uint64_t ns)
{
    cfg_.dutyCycleNs = ns;
    return *this;
}

PWM::Builder& PWM::Builder::enabled(bool on)
{
    cfg_.enabled = on;
    return *this;
}

PWM PWM::Builder::build()
{
    return PWM(cfg_);
}

PWM PWM::Builder::buildAndOpen(OpenMode mode)
{
    auto dev = build();
    dev.open(mode);
    return dev;
}

PWM::Builder PWM::on(std::string chipPath)
{
    return Builder(std::move(chipPath));
}

PWM::PWM(PWMConfig config) : config_(std::move(config)) {}

PWM::~PWM()
{
    close();
}

PWM::PWM(PWM&& other) noexcept
    : config_(std::move(other.config_))
    , channelPath_(std::move(other.channelPath_))
    , state_(other.state_)
    , exported_(other.exported_)
{
    other.state_    = DeviceState::Closed;
    other.exported_ = false;
}

PWM& PWM::operator=(PWM&& other) noexcept
{
    if (this != &other) {
        close();
        config_      = std::move(other.config_);
        channelPath_ = std::move(other.channelPath_);
        state_       = other.state_;
        exported_    = other.exported_;
        other.state_    = DeviceState::Closed;
        other.exported_ = false;
    }
    return *this;
}

std::string PWM::pwmPath() const
{
    return channelPath_;
}

void PWM::writeSysfs(const std::string& attr, std::string_view value)
{
    writeFile(pwmPath() + "/" + attr, value);
}

void PWM::exportChannel()
{
    if (exported_) {
        return;
    }

    std::ostringstream oss;
    oss << config_.channel;
    writeFile(config_.chipPath + "/export", oss.str());

    channelPath_ = config_.chipPath + "/pwm" + oss.str();
    exported_      = true;
}

void PWM::unexportChannel() noexcept
{
    if (!exported_) {
        return;
    }

    try {
        std::ostringstream oss;
        oss << config_.channel;
        writeFile(config_.chipPath + "/unexport", oss.str());
    } catch (...) {
    }
    exported_    = false;
    channelPath_.clear();
}

void PWM::open(OpenMode mode)
{
    (void)mode;
    if (state_ == DeviceState::Open) {
        return;
    }

    if (config_.chipPath.empty()) {
        throw DeviceError(std::errc::invalid_argument, "PWM chip path is empty");
    }

    try {
        exportChannel();
        setPeriodNs(config_.periodNs);
        setDutyCycleNs(config_.dutyCycleNs);
        setEnabled(config_.enabled);
    } catch (...) {
        close();
        throw;
    }
    state_ = DeviceState::Open;
}

void PWM::close() noexcept
{
    if (state_ == DeviceState::Open && exported_) {
        try {
            setEnabled(false);
        } catch (...) {
        }
    }
    unexportChannel();
    state_ = DeviceState::Closed;
}

DeviceState PWM::state() const noexcept
{
    return state_;
}

std::string_view PWM::path() const noexcept
{
    return channelPath_.empty() ? config_.chipPath : channelPath_;
}

std::size_t PWM::read(std::span<std::byte>)
{
    throw DeviceError(std::errc::operation_not_supported, "PWM sysfs write-only control");
}

std::size_t PWM::write(std::span<const std::byte>)
{
    throw DeviceError(std::errc::operation_not_supported, "use setPeriodNs/setDutyCycleNs");
}

void PWM::setPeriodNs(uint64_t ns)
{
    if (!exported_ && state_ != DeviceState::Open) {
        throw DeviceNotOpenError(config_.chipPath);
    }
    writeSysfs("period", std::to_string(ns));
    config_.periodNs = ns;
}

void PWM::setDutyCycleNs(uint64_t ns)
{
    if (!exported_ && state_ != DeviceState::Open) {
        throw DeviceNotOpenError(config_.chipPath);
    }
    writeSysfs("duty_cycle", std::to_string(ns));
    config_.dutyCycleNs = ns;
}

void PWM::setEnabled(bool on)
{
    if (!exported_ && state_ != DeviceState::Open) {
        throw DeviceNotOpenError(config_.chipPath);
    }
    writeSysfs("enable", on ? "1" : "0");
    config_.enabled = on;
}

const PWMConfig& PWM::config() const noexcept
{
    return config_;
}

}  // namespace lpio
