#pragma once

#include "lpio/private/FdDeviceBase.hpp"

#include <cstdint>
#include <string>
#include <string_view>

namespace lpio {

struct SPIBusConfig {
    uint32_t speedHz      = 1'000'000;
    uint8_t  mode         = 0;
    uint8_t  bitsPerWord  = 8;
    uint16_t delayUsecs   = 0;
};

class SPIBus final : public detail::FdDeviceBase {
public:
    class Builder {
    public:
        explicit Builder(std::string devPath);

        Builder& speedHz(uint32_t hz);
        Builder& mode(uint8_t mode);
        Builder& bitsPerWord(uint8_t bits);
        Builder& delayUsecs(uint16_t us);

        SPIBus build();
        SPIBus buildAndOpen(OpenMode mode = OpenMode::ReadWrite);

    private:
        std::string   devPath_;
        SPIBusConfig  cfg_;
    };

    static Builder on(std::string devPath);

    SPIBus(std::string devPath, SPIBusConfig config);
    ~SPIBus() override;

    SPIBus(const SPIBus&)            = delete;
    SPIBus& operator=(const SPIBus&) = delete;
    SPIBus(SPIBus&& other) noexcept;
    SPIBus& operator=(SPIBus&& other) noexcept;

    void open(OpenMode mode = OpenMode::ReadWrite) override;
    void close() noexcept override;

    DeviceState      state() const noexcept override;
    std::string_view path() const noexcept override;

    std::size_t read(std::span<std::byte> buf) override;
    std::size_t write(std::span<const std::byte> buf) override;

    std::size_t transfer(const void* tx, void* rx, std::size_t len);

    const SPIBusConfig& config() const noexcept;

private:
    void applySpiSettings();

    std::string  devPath_;
    SPIBusConfig config_;
    DeviceState  state_ = DeviceState::Closed;
};

}  // namespace lpio
