#pragma once

#include "lpio/DeviceBase.hpp"

#include <cstdint>
#include <string>
#include <string_view>

namespace lpio {

struct PWMConfig {
    std::string chipPath;   // e.g. "/sys/class/pwm/pwmchip0"
    uint32_t    channel    = 0;
    uint64_t    periodNs   = 1'000'000;   // 1 kHz default
    uint64_t    dutyCycleNs = 0;
    bool        enabled    = false;
};

class PWM final : public DeviceBase {
public:
    class Builder {
    public:
        explicit Builder(std::string chipPath);

        Builder& channel(uint32_t ch);
        Builder& periodNs(uint64_t ns);
        Builder& dutyCycleNs(uint64_t ns);
        Builder& enabled(bool on = true);

        PWM build();
        PWM buildAndOpen(OpenMode mode = OpenMode::ReadWrite);

    private:
        PWMConfig cfg_;
    };

    static Builder on(std::string chipPath);

    explicit PWM(PWMConfig config);
    ~PWM() override;

    PWM(const PWM&)            = delete;
    PWM& operator=(const PWM&) = delete;
    PWM(PWM&& other) noexcept;
    PWM& operator=(PWM&& other) noexcept;

    void open(OpenMode mode = OpenMode::ReadWrite) override;
    void close() noexcept override;

    DeviceState      state() const noexcept override;
    std::string_view path() const noexcept override;

    std::size_t read(std::span<std::byte> buf) override;
    std::size_t write(std::span<const std::byte> buf) override;

    void setPeriodNs(uint64_t ns);
    void setDutyCycleNs(uint64_t ns);
    void setEnabled(bool on);

    const PWMConfig& config() const noexcept;

private:
    void exportChannel();
    void unexportChannel() noexcept;
    void writeSysfs(const std::string& attr, std::string_view value);
    std::string pwmPath() const;

    PWMConfig   config_;
    std::string channelPath_;
    DeviceState state_ = DeviceState::Closed;
    bool        exported_ = false;
};

}  // namespace lpio
