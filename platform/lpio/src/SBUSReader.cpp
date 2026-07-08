#include "lpio/SBUSReader.hpp"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <thread>
#include <utility>

namespace lpio {

namespace {

SerialPortConfig makeSbusLineConfig(const SbusConfig& cfg)
{
    SerialPortConfig sp {};
    sp.baudRate     = cfg.baudRate;
    sp.dataBits     = 8;
    sp.stopBits     = 2;
    sp.parity       = SerialParity::Even;
    sp.nonBlocking  = cfg.nonBlocking;
    return sp;
}

}  // namespace

const std::array<SbusChannelInfo, kSbusChannelCount>& SBUSReader::defaultChannelMap()
{
    static const std::array<SbusChannelInfo, kSbusChannelCount> kMap = {{
        {0,  "CH1",  "Roll",     kSbusPwmMin, kSbusPwmMax, kSbusPwmCenter},
        {1,  "CH2",  "Pitch",    kSbusPwmMin, kSbusPwmMax, kSbusPwmCenter},
        {2,  "CH3",  "Throttle", kSbusPwmMin, kSbusPwmMax, kSbusPwmMin},
        {3,  "CH4",  "Yaw",      kSbusPwmMin, kSbusPwmMax, kSbusPwmCenter},
        {4,  "CH5",  "AUX1",     kSbusPwmMin, kSbusPwmMax, kSbusPwmCenter},
        {5,  "CH6",  "AUX2",     kSbusPwmMin, kSbusPwmMax, kSbusPwmCenter},
        {6,  "CH7",  "AUX3",     kSbusPwmMin, kSbusPwmMax, kSbusPwmCenter},
        {7,  "CH8",  "AUX4",     kSbusPwmMin, kSbusPwmMax, kSbusPwmCenter},
        {8,  "CH9",  "AUX5",     kSbusPwmMin, kSbusPwmMax, kSbusPwmCenter},
        {9,  "CH10", "AUX6",     kSbusPwmMin, kSbusPwmMax, kSbusPwmCenter},
        {10, "CH11", "AUX7",     kSbusPwmMin, kSbusPwmMax, kSbusPwmCenter},
        {11, "CH12", "AUX8",     kSbusPwmMin, kSbusPwmMax, kSbusPwmCenter},
        {12, "CH13", "AUX9",     kSbusPwmMin, kSbusPwmMax, kSbusPwmCenter},
        {13, "CH14", "AUX10",    kSbusPwmMin, kSbusPwmMax, kSbusPwmCenter},
        {14, "CH15", "AUX11",    kSbusPwmMin, kSbusPwmMax, kSbusPwmCenter},
        {15, "CH16", "AUX12",    kSbusPwmMin, kSbusPwmMax, kSbusPwmCenter},
    }};
    return kMap;
}

const std::array<SbusSwitchInfo, kSbusSwitchCount>& SBUSReader::defaultSwitchMap()
{
    static const std::array<SbusSwitchInfo, kSbusSwitchCount> kMap = {{
        {0, "SW1", "Digital switch 1", 0, 1},
        {1, "SW2", "Digital switch 2", 0, 1},
    }};
    return kMap;
}

SBUSReader::Builder::Builder(std::string path) : path_(std::move(path)) {}

SBUSReader::Builder& SBUSReader::Builder::baudRate(uint32_t rate)
{
    cfg_.baudRate = rate;
    return *this;
}

SBUSReader::Builder& SBUSReader::Builder::nonBlocking(bool enable)
{
    cfg_.nonBlocking = enable;
    return *this;
}

SBUSReader SBUSReader::Builder::build()
{
    return SBUSReader(path_, cfg_);
}

SBUSReader SBUSReader::Builder::buildAndOpen(OpenMode mode)
{
    auto dev = build();
    dev.open(mode);
    return dev;
}

SBUSReader::Builder SBUSReader::on(std::string path)
{
    return Builder(std::move(path));
}

SBUSReader::SBUSReader(std::string path, SbusConfig config)
    : port_(std::move(path), makeSbusLineConfig(config))
    , sbusCfg_(config)
{
    useDefaultChannelMap();
}

SBUSReader::SBUSReader(SBUSReader&& other) noexcept
    : port_(std::move(other.port_))
    , sbusCfg_(other.sbusCfg_)
    , channels_(std::move(other.channels_))
    , switches_(std::move(other.switches_))
{}

SBUSReader& SBUSReader::operator=(SBUSReader&& other) noexcept
{
    if (this != &other) {
        port_ = std::move(other.port_);
        sbusCfg_   = other.sbusCfg_;
        channels_  = std::move(other.channels_);
        switches_  = std::move(other.switches_);
    }
    return *this;
}

void SBUSReader::open(OpenMode mode)
{
    port_.open(mode);
}

void SBUSReader::close() noexcept
{
    port_.close();
}

DeviceState SBUSReader::state() const noexcept
{
    return port_.state();
}

std::string_view SBUSReader::path() const noexcept
{
    return port_.path();
}

std::size_t SBUSReader::read(std::span<std::byte>)
{
    throw DeviceError(std::errc::operation_not_supported, "use recvFrame()");
}

std::size_t SBUSReader::write(std::span<const std::byte>)
{
    throw DeviceError(std::errc::operation_not_supported, "SBUSReader is receive-only");
}

void SBUSReader::useDefaultChannelMap()
{
    const auto& map = defaultChannelMap();
    channels_.assign(map.begin(), map.end());

    const auto& sw = defaultSwitchMap();
    switches_.assign(sw.begin(), sw.end());
}

void SBUSReader::registerChannels(std::span<const SbusChannelInfo> channels)
{
    if (channels.size() != kSbusChannelCount) {
        throw DeviceError(std::errc::invalid_argument, "SBUSReader: expected 16 channel descriptors");
    }
    channels_.assign(channels.begin(), channels.end());
}

void SBUSReader::registerSwitches(std::span<const SbusSwitchInfo> switches)
{
    if (switches.size() != kSbusSwitchCount) {
        throw DeviceError(std::errc::invalid_argument, "SBUSReader: expected 2 switch descriptors");
    }
    switches_.assign(switches.begin(), switches.end());
}

const std::vector<SbusChannelInfo>& SBUSReader::channels() const noexcept
{
    return channels_;
}

const std::vector<SbusSwitchInfo>& SBUSReader::switches() const noexcept
{
    return switches_;
}

float SBUSReader::rawToNormalized(uint16_t raw, const SbusChannelInfo& info)
{
    if (raw <= info.centerValue) {
        const auto denom = static_cast<float>(info.centerValue - info.minValue);
        if (denom <= 0.0f) {
            return 0.0f;
        }
        return -1.0f + (static_cast<float>(raw - info.minValue) / denom);
    }

    const auto denom = static_cast<float>(info.maxValue - info.centerValue);
    if (denom <= 0.0f) {
        return 0.0f;
    }
    return (static_cast<float>(raw - info.centerValue) / denom);
}

float SBUSReader::rawToPercent(uint16_t raw, const SbusChannelInfo& info)
{
    const auto range = static_cast<float>(info.maxValue - info.minValue);
    if (range <= 0.0f) {
        return 0.0f;
    }
    return 100.0f * static_cast<float>(raw - info.minValue) / range;
}

float SBUSReader::rawToMicroseconds(uint16_t raw, const SbusChannelInfo& info)
{
    const auto range = static_cast<float>(info.maxValue - info.minValue);
    if (range <= 0.0f) {
        return 1500.0f;
    }
    return 1000.0f + (static_cast<float>(raw - info.minValue) * 1000.0f / range);
}

SbusFrame SBUSReader::decodeFrame(const uint8_t raw[kSbusFrameBytes]) const
{
    SbusFrame frame {};
    std::memcpy(frame.raw.data(), raw, kSbusFrameBytes);

    for (std::size_t i = 0; i < kSbusChannelCount; ++i) {
        const int bitIndex  = static_cast<int>(i * 11);
        const int byteIndex = 1 + (bitIndex / 8);
        const int bitOffset = bitIndex % 8;

        const uint16_t v = static_cast<uint16_t>(raw[byteIndex] >> bitOffset) |
                           static_cast<uint16_t>(raw[byteIndex + 1] << (8 - bitOffset));
        frame.channels[i] = static_cast<uint16_t>(v & 0x07FFu);
    }

    const uint8_t flags = raw[23];
    frame.frameLost     = (flags & 0x04u) != 0;
    frame.failsafe      = (flags & 0x08u) != 0;
    frame.switches[0]   = (flags & 0x10u) ? 1u : 0u;
    frame.switches[1]   = (flags & 0x20u) ? 1u : 0u;

    return frame;
}

uint8_t SBUSReader::readByte()
{
    std::byte b {};
    const std::size_t n = port_.read(std::span<std::byte>(&b, 1));
    if (n == 0) {
        throw DeviceTimeoutError("SBUSReader readByte");
    }
    return static_cast<uint8_t>(b);
}

SbusFrame SBUSReader::recvFrame(std::chrono::milliseconds timeout)
{
    requireOpen();

    using Clock = std::chrono::steady_clock;
    const auto deadline = Clock::now() + timeout;

    while (Clock::now() < deadline) {
        uint8_t b = 0;
        try {
            b = readByte();
        } catch (const DeviceTimeoutError&) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }

        if (b != 0x0F) {
            continue;
        }

        uint8_t raw[kSbusFrameBytes] {};
        raw[0] = b;

        std::size_t got = 1;
        while (got < kSbusFrameBytes) {
            if (Clock::now() >= deadline) {
                throw DeviceTimeoutError("SBUSReader recvFrame");
            }

            try {
                const std::size_t n =
                    port_.read(std::span<std::byte>(reinterpret_cast<std::byte*>(raw + got),
                                                    kSbusFrameBytes - got));
                got += n;
            } catch (const DeviceTimeoutError&) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }

        if (raw[kSbusFrameBytes - 1] != 0x00) {
            continue;
        }

        return decodeFrame(raw);
    }

    throw DeviceTimeoutError("SBUSReader recvFrame");
}

const SbusConfig& SBUSReader::sbusConfig() const noexcept
{
    return sbusCfg_;
}

}  // namespace lpio
