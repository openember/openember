#pragma once

#include "lpio/private/FdDeviceBase.hpp"

#include <array>
#include <chrono>
#include <cstdint>
#include <string>
#include <string_view>

namespace lpio {

struct CANBusConfig {
    bool enableCanFd = false;
};

struct CANFrame {
    uint32_t              canId      = 0;
    uint8_t               dataLen    = 0;
    bool                  isExtended = false;
    bool                  isFd       = false;
    std::array<uint8_t, 64> data {};
};

class CANBus final : public detail::FdDeviceBase {
public:
    class Builder {
    public:
        explicit Builder(std::string ifname);

        Builder& canFd(bool enable = true);

        CANBus build();
        CANBus buildAndOpen(OpenMode mode = OpenMode::ReadWrite);

    private:
        std::string   ifname_;
        CANBusConfig  cfg_;
    };

    static Builder on(std::string ifname);

    CANBus(std::string ifname, CANBusConfig config);
    ~CANBus() override;

    CANBus(const CANBus&)            = delete;
    CANBus& operator=(const CANBus&) = delete;
    CANBus(CANBus&& other) noexcept;
    CANBus& operator=(CANBus&& other) noexcept;

    void open(OpenMode mode = OpenMode::ReadWrite) override;
    void close() noexcept override;

    DeviceState      state() const noexcept override;
    std::string_view path() const noexcept override;

    std::size_t read(std::span<std::byte> buf) override;
    std::size_t write(std::span<const std::byte> buf) override;

    void sendFrame(const CANFrame& frame);
    CANFrame recvFrame(std::chrono::milliseconds timeout = std::chrono::milliseconds(1000));

    const CANBusConfig& config() const noexcept;

private:
    std::string   ifname_;
    CANBusConfig  config_;
    DeviceState   state_ = DeviceState::Closed;
    bool          canFdEnabled_ = false;
};

}  // namespace lpio
