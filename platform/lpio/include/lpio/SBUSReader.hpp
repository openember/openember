#pragma once

#include "lpio/DeviceBase.hpp"
#include "lpio/SerialPort.hpp"

#include <array>
#include <chrono>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace lpio {

/** SBUS 11-bit PWM 通道标准量程（Futaba / FrSky 兼容） */
inline constexpr uint16_t kSbusPwmMin    = 172;
inline constexpr uint16_t kSbusPwmMax    = 1811;
inline constexpr uint16_t kSbusPwmCenter = 992;

inline constexpr uint32_t kSbusDefaultBaudRate = 100000;
inline constexpr std::size_t kSbusChannelCount = 16;
inline constexpr std::size_t kSbusSwitchCount  = 2;
inline constexpr std::size_t kSbusFrameBytes   = 25;

/** 单路 PWM 通道元数据（用于显示、归一化、越界检测） */
struct SbusChannelInfo {
    uint8_t       index       = 0;
    std::string_view name;
    std::string_view label;
    uint16_t      minValue    = kSbusPwmMin;
    uint16_t      maxValue    = kSbusPwmMax;
    uint16_t      centerValue = kSbusPwmCenter;
};

/** 数字开关通道元数据 */
struct SbusSwitchInfo {
    uint8_t       index    = 0;
    std::string_view name;
    std::string_view label;
    uint8_t       offValue = 0;
    uint8_t       onValue  = 1;
};

struct SbusConfig {
    uint32_t baudRate    = kSbusDefaultBaudRate;
    bool     nonBlocking = true;
};

struct SbusFrame {
    std::array<uint16_t, kSbusChannelCount> channels {};
    std::array<uint8_t, kSbusSwitchCount>  switches {};
    bool                                   frameLost = false;
    bool                                   failsafe  = false;
    std::array<uint8_t, kSbusFrameBytes>   raw {};
};

class SBUSReader final : public DeviceBase {
public:
    class Builder {
    public:
        explicit Builder(std::string path);

        Builder& baudRate(uint32_t rate);
        Builder& nonBlocking(bool enable = true);

        SBUSReader build();
        SBUSReader buildAndOpen(OpenMode mode = OpenMode::ReadWrite);

    private:
        std::string path_;
        SbusConfig  cfg_;
    };

    static Builder on(std::string path);

    SBUSReader(std::string path, SbusConfig config);
    ~SBUSReader() override = default;

    SBUSReader(const SBUSReader&)            = delete;
    SBUSReader& operator=(const SBUSReader&) = delete;
    SBUSReader(SBUSReader&& other) noexcept;
    SBUSReader& operator=(SBUSReader&& other) noexcept;

    void open(OpenMode mode = OpenMode::ReadWrite) override;
    void close() noexcept override;

    DeviceState      state() const noexcept override;
    std::string_view path() const noexcept override;

    std::size_t read(std::span<std::byte> buf) override;
    std::size_t write(std::span<const std::byte> buf) override;

    /** 使用默认飞控通道表（CH1–CH4: 横滚/俯仰/油门/偏航，CH5–CH16: AUX） */
    void useDefaultChannelMap();

    /** 注册/替换全部 PWM 通道描述（数量须为 kSbusChannelCount） */
    void registerChannels(std::span<const SbusChannelInfo> channels);

    /** 注册/替换数字开关描述（数量须为 kSbusSwitchCount） */
    void registerSwitches(std::span<const SbusSwitchInfo> switches);

    const std::vector<SbusChannelInfo>& channels() const noexcept;
    const std::vector<SbusSwitchInfo>&  switches() const noexcept;

    SbusFrame recvFrame(std::chrono::milliseconds timeout = std::chrono::milliseconds(1000));

    const SbusConfig& sbusConfig() const noexcept;

    // ── 换算辅助（基于通道 min/max/center）────────────────
    static float rawToNormalized(uint16_t raw, const SbusChannelInfo& info);
    static float rawToPercent(uint16_t raw, const SbusChannelInfo& info);
    static float rawToMicroseconds(uint16_t raw, const SbusChannelInfo& info);

    static const std::array<SbusChannelInfo, kSbusChannelCount>& defaultChannelMap();
    static const std::array<SbusSwitchInfo, kSbusSwitchCount>&  defaultSwitchMap();

private:
    SbusFrame decodeFrame(const uint8_t raw[kSbusFrameBytes]) const;
    uint8_t   readByte();

    SerialPort                   port_;
    SbusConfig                   sbusCfg_;
    std::vector<SbusChannelInfo> channels_;
    std::vector<SbusSwitchInfo>  switches_;
};

}  // namespace lpio
