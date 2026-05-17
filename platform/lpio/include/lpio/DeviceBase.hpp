/* Copyright (c) 2026, OpenEmber Team
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2026-05-18     luhuadong    the first version
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <span>  // C++20
#include <string>
#include <string_view>
#include <chrono>
#include <stdexcept>
#include <system_error>
#include <thread>

namespace lpio {

// ─────────────────────────────────────────────
// 错误类型
// ─────────────────────────────────────────────

class DeviceError : public std::system_error {
public:
    explicit DeviceError(std::errc code, std::string_view context = "")
        : std::system_error(std::make_error_code(code),
                            context.empty() ? "" : std::string(context))
    {}

    explicit DeviceError(int errno_val, std::string_view context = "")
        : std::system_error(errno_val, std::generic_category(),
                            context.empty() ? "" : std::string(context))
    {}
};

class DeviceNotOpenError : public DeviceError {
public:
    explicit DeviceNotOpenError(std::string_view path = "")
        : DeviceError(std::errc::not_connected,
                      path.empty() ? "device not open"
                                   : "device not open: " + std::string(path))
    {}
};

class DeviceTimeoutError : public DeviceError {
public:
    explicit DeviceTimeoutError(std::string_view context = "")
        : DeviceError(std::errc::timed_out, context)
    {}
};

// ─────────────────────────────────────────────
// 设备状态
// ─────────────────────────────────────────────

enum class DeviceState {
    Closed,    // 未打开
    Open,      // 正常运行
    Error,     // 遇到错误，需 close() 后重新 open()
};

// ─────────────────────────────────────────────
// 打开模式标志（可按位组合）
// ─────────────────────────────────────────────

enum class OpenMode : uint32_t {
    ReadOnly  = 0x01,
    WriteOnly = 0x02,
    ReadWrite = 0x03,   // ReadOnly | WriteOnly
    NonBlock  = 0x04,   // 非阻塞模式
};

inline OpenMode operator|(OpenMode a, OpenMode b) {
    return static_cast<OpenMode>(
        static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}

inline bool operator&(OpenMode a, OpenMode b) {
    return (static_cast<uint32_t>(a) & static_cast<uint32_t>(b)) != 0;
}

// ─────────────────────────────────────────────
// DeviceBase — 所有设备驱动的抽象基类
// ─────────────────────────────────────────────

class DeviceBase {
public:
    // 禁止拷贝，允许移动（子类自行决定是否支持移动）
    DeviceBase(const DeviceBase&)            = delete;
    DeviceBase& operator=(const DeviceBase&) = delete;
    DeviceBase(DeviceBase&&)                 = default;
    DeviceBase& operator=(DeviceBase&&)      = default;

    // 虚析构：确保通过基类指针 delete 时行为正确
    // 析构时自动 close，子类析构函数中也应调用 close()
    virtual ~DeviceBase() = default;

    // ── 生命周期 ──────────────────────────────

    // 打开设备。失败抛 DeviceError
    virtual void open(OpenMode mode = OpenMode::ReadWrite) = 0;

    // 关闭设备。允许对已关闭的设备调用（幂等）
    virtual void close() noexcept = 0;

    // ── 状态查询 ──────────────────────────────

    virtual DeviceState state() const noexcept = 0;

    bool isOpen() const noexcept {
        return state() == DeviceState::Open;
    }

    // 设备路径或名称，如 "/dev/ttyS0"、"can0"
    virtual std::string_view path() const noexcept = 0;

    // ── 读写 ──────────────────────────────────

    // 读取最多 buf.size() 字节，返回实际读取字节数
    // 非阻塞模式下可能返回 0；失败抛 DeviceError
    virtual std::size_t read(std::span<std::byte> buf) = 0;

    // 写入 buf 中全部字节，返回实际写入字节数
    // 失败抛 DeviceError
    virtual std::size_t write(std::span<const std::byte> buf) = 0;

    // ── 便捷重载（非虚，基于上面两个虚函数实现）──

    std::size_t read(std::span<uint8_t> buf) {
        return read(std::as_writable_bytes(buf));
    }

    std::size_t write(std::span<const uint8_t> buf) {
        return write(std::as_bytes(buf));
    }

    // ── 超时读写（有默认实现，子类可覆盖优化）──

    // 阻塞直到读满 buf 或超时，返回实际读取字节数
    virtual std::size_t readExact(
        std::span<std::byte> buf,
        std::chrono::milliseconds timeout = std::chrono::milliseconds(1000))
    {
        // 默认实现：轮询 + 睡眠（子类应覆盖为 select/poll 实现）
        using Clock = std::chrono::steady_clock;
        auto deadline = Clock::now() + timeout;
        std::size_t total = 0;

        while (total < buf.size()) {
            if (Clock::now() >= deadline)
                throw DeviceTimeoutError("readExact");

            auto n = read(buf.subspan(total));
            total += n;

            if (n == 0)
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        return total;
    }

    // 写出全部字节，失败或超时抛异常
    virtual std::size_t writeAll(
        std::span<const std::byte> buf,
        std::chrono::milliseconds timeout = std::chrono::milliseconds(1000))
    {
        using Clock = std::chrono::steady_clock;
        auto deadline = Clock::now() + timeout;
        std::size_t total = 0;

        while (total < buf.size()) {
            if (Clock::now() >= deadline)
                throw DeviceTimeoutError("writeAll");

            auto n = write(buf.subspan(total));
            total += n;
        }
        return total;
    }

    // ── 控制接口（可选实现）──────────────────

    // 刷新输入/输出缓冲区，默认空实现
    virtual void flush() {}

    // 检查可读字节数，-1 表示不支持
    virtual int available() const noexcept { return -1; }

protected:
    // 子类通过 protected 构造函数初始化
    DeviceBase() = default;

    // 供子类检查设备是否已打开，未开则抛异常
    void requireOpen() const {
        if (!isOpen())
            throw DeviceNotOpenError(path());
    }
};

} // namespace lpio