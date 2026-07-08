#pragma once

#include "lpio/private/FdDeviceBase.hpp"

#include <cstdint>
#include <string>
#include <string_view>

namespace lpio {

struct I2CBusConfig {
    uint32_t busNum    = 0;
    uint8_t  addr7bit  = 0;
};

class I2CBus final : public detail::FdDeviceBase {
public:
    class Builder {
    public:
        Builder& bus(uint32_t busNum);
        Builder& address(uint8_t addr7bit);

        I2CBus build();
        I2CBus buildAndOpen(OpenMode mode = OpenMode::ReadWrite);

    private:
        I2CBusConfig cfg_;
    };

    static Builder create();

    explicit I2CBus(I2CBusConfig config);
    ~I2CBus() override;

    I2CBus(const I2CBus&)            = delete;
    I2CBus& operator=(const I2CBus&) = delete;
    I2CBus(I2CBus&& other) noexcept;
    I2CBus& operator=(I2CBus&& other) noexcept;

    void open(OpenMode mode = OpenMode::ReadWrite) override;
    void close() noexcept override;

    DeviceState      state() const noexcept override;
    std::string_view path() const noexcept override;

    std::size_t read(std::span<std::byte> buf) override;
    std::size_t write(std::span<const std::byte> buf) override;

    std::size_t writeRead(const uint8_t* wbuf, std::size_t wlen, uint8_t* rbuf, std::size_t rlen);

    const I2CBusConfig& config() const noexcept;

private:
    void refreshDevPath();

    I2CBusConfig config_;
    std::string  devPath_;
    DeviceState  state_ = DeviceState::Closed;
};

}  // namespace lpio
