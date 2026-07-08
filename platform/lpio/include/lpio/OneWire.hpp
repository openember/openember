#pragma once

#include "lpio/DeviceBase.hpp"

#include <cstdint>
#include <string>
#include <string_view>

namespace lpio {

struct OneWireTemp {
    bool    crcOk            = false;
    int32_t temperatureMilliC = 0;
    float   temperatureC     = 0.0f;
};

class OneWire final : public DeviceBase {
public:
    class Builder {
    public:
        explicit Builder(std::string slavePath);

        OneWire build();
        OneWire open(OpenMode mode = OpenMode::ReadOnly);

    private:
        std::string slavePath_;
    };

    static Builder slave(std::string slavePath);

    explicit OneWire(std::string slavePath);
    ~OneWire() override;

    OneWire(const OneWire&)            = delete;
    OneWire& operator=(const OneWire&) = delete;
    OneWire(OneWire&& other) noexcept;
    OneWire& operator=(OneWire&& other) noexcept;

    void open(OpenMode mode = OpenMode::ReadOnly) override;
    void close() noexcept override;

    DeviceState      state() const noexcept override;
    std::string_view path() const noexcept override;

    std::size_t read(std::span<std::byte> buf) override;
    std::size_t write(std::span<const std::byte> buf) override;

    OneWireTemp readTemperature();
    std::string readRaw();

    const std::string& slavePath() const noexcept;

private:
    std::string  slavePath_;
    DeviceState  state_ = DeviceState::Closed;
};

}  // namespace lpio
