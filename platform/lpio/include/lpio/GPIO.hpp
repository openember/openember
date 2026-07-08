// include/lpio/GPIO.hpp
#pragma once

#include "lpio/DeviceBase.hpp"
#include "lpio/private/UniqueFd.hpp"

#include <cstdint>
#include <string>
#include <string_view>
#include <chrono>
#include <optional>

namespace lpio {

// ─────────────────────────────────────────────
// GPIO 专属错误
// ─────────────────────────────────────────────

class GPIOError : public DeviceError {
public:
    explicit GPIOError(int errno_val, std::string_view context = "")
        : DeviceError(errno_val, context)
    {}

    explicit GPIOError(std::errc code, std::string_view context = "")
        : DeviceError(code, context)
    {}
};

// ─────────────────────────────────────────────
// 枚举定义
// ─────────────────────────────────────────────

enum class GPIODirection {
    Input,
    Output,
};

enum class GPIOValue {
    Low  = 0,
    High = 1,
};

enum class GPIOEdge {
    None,
    Rising,
    Falling,
    Both,
};

enum class GPIOBias {
    Disabled,   // 无上下拉
    PullUp,     // 上拉
    PullDown,   // 下拉
    AsIs,       // 保持硬件当前状态
};

enum class GPIODrive {
    PushPull,       // 推挽输出（默认）
    OpenDrain,      // 开漏
    OpenSource,     // 开源
};

// ─────────────────────────────────────────────
// GPIO 配置
// ─────────────────────────────────────────────

struct GPIOConfig {
    uint32_t      line;                              // GPIO 线号（chip 内偏移）
    GPIODirection direction  = GPIODirection::Input;
    GPIOValue     initValue  = GPIOValue::Low;       // 仅 Output 时有效
    GPIOBias      bias       = GPIOBias::Disabled;
    GPIODrive     drive      = GPIODrive::PushPull;
    bool          activeLow  = false;                // 反转有效电平
    std::string   consumer   = "lpio";               // 标识占用者，调试用
};

// ─────────────────────────────────────────────
// GPIO 类
// ─────────────────────────────────────────────

class GPIO final : public DeviceBase {
public:
    class Builder {
    public:
        explicit Builder(std::string chipPath);

        Builder& line(uint32_t line);
        Builder& direction(GPIODirection dir);
        Builder& initValue(GPIOValue value);
        Builder& bias(GPIOBias b);
        Builder& drive(GPIODrive d);
        Builder& activeLow(bool enable = true);
        Builder& consumer(std::string name);

        GPIO build();
        GPIO open(OpenMode mode = OpenMode::ReadWrite);

    private:
        std::string chipPath_;
        GPIOConfig  cfg_;
    };

    static Builder on(std::string chipPath);

    // chipPath: gpiochip 路径，如 "/dev/gpiochip0"
    explicit GPIO(std::string chipPath, GPIOConfig config);
    ~GPIO() override;

    // 禁止拷贝，允许移动
    GPIO(const GPIO&)            = delete;
    GPIO& operator=(const GPIO&) = delete;
    GPIO(GPIO&& other) noexcept;
    GPIO& operator=(GPIO&& other) noexcept;

    // ── DeviceBase 接口 ───────────────────────

    void open(OpenMode mode = OpenMode::ReadWrite) override;
    void close() noexcept override;

    DeviceState      state() const noexcept override;
    std::string_view path()  const noexcept override;

    // GPIO 不是字节流设备；这两个接口语义不适用，调用时抛 GPIOError
    std::size_t read(std::span<std::byte> buf)        override;
    std::size_t write(std::span<const std::byte> buf) override;

    // ── GPIO 专属接口 ─────────────────────────

    // 读取当前电平
    GPIOValue get() const;

    // 设置输出电平（仅 Output 方向有效）
    void set(GPIOValue value);

    // 便捷方法
    void setHigh() { set(GPIOValue::High); }
    void setLow()  { set(GPIOValue::Low);  }
    void toggle();

    // 读取方向
    GPIODirection direction() const noexcept;

    // 运行时切换方向（重新请求 line）
    void setDirection(GPIODirection dir, GPIOValue initValue = GPIOValue::Low);

    // 等待边沿触发，返回触发时的电平；超时返回 std::nullopt
    // 仅 Input 方向有效
    std::optional<GPIOValue> waitForEdge(
        GPIOEdge edge,
        std::chrono::milliseconds timeout = std::chrono::milliseconds(1000));

    // 获取当前配置
    const GPIOConfig& config() const noexcept;

private:
    static detail::UniqueFd requestLine(int chipFd, const GPIOConfig& config, std::string_view chipPath);
    void releaseLine() noexcept;

    std::string  chipPath_;
    GPIOConfig   config_;
    DeviceState  state_ = DeviceState::Closed;

    detail::UniqueFd chipFd_;   // gpiochip 文件描述符（open 后持有）
    detail::UniqueFd lineFd_;   // 请求到的 line 文件描述符
};

} // namespace lpio
