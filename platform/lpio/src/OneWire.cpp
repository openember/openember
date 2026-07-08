#include "lpio/OneWire.hpp"
#include "lpio/private/UniqueFd.hpp"

#include <algorithm>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>

namespace lpio {

namespace {

[[noreturn]] void throwErrno(std::string_view context)
{
    throw DeviceError(errno, context);
}

std::string readFile(const std::string& path)
{
    detail::UniqueFd fd(::open(path.c_str(), O_RDONLY));
    if (!fd) {
        throwErrno(path);
    }

    std::string out;
    char        buf[256];
    while (true) {
        const ssize_t n = ::read(fd.get(), buf, sizeof(buf));
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            throwErrno(path);
        }
        if (n == 0) {
            break;
        }
        out.append(buf, static_cast<std::size_t>(n));
    }
    return out;
}

}  // namespace

OneWire::Builder::Builder(std::string slavePath) : slavePath_(std::move(slavePath)) {}

OneWire OneWire::Builder::build()
{
    return OneWire(slavePath_);
}

OneWire OneWire::Builder::buildAndOpen(OpenMode mode)
{
    auto dev = build();
    dev.open(mode);
    return dev;
}

OneWire::Builder OneWire::slave(std::string slavePath)
{
    return Builder(std::move(slavePath));
}

OneWire::OneWire(std::string slavePath) : slavePath_(std::move(slavePath)) {}

OneWire::~OneWire() = default;

OneWire::OneWire(OneWire&& other) noexcept
    : slavePath_(std::move(other.slavePath_))
    , state_(other.state_)
{
    other.state_ = DeviceState::Closed;
}

OneWire& OneWire::operator=(OneWire&& other) noexcept
{
    if (this != &other) {
        close();
        slavePath_ = std::move(other.slavePath_);
        state_     = other.state_;
        other.state_ = DeviceState::Closed;
    }
    return *this;
}

void OneWire::open(OpenMode mode)
{
    (void)mode;
    if (state_ == DeviceState::Open) {
        return;
    }
    if (slavePath_.empty()) {
        throw DeviceError(std::errc::invalid_argument, "1-wire slave path is empty");
    }

    const std::string w1Path = slavePath_ + "/w1_slave";
    (void)readFile(w1Path);
    state_ = DeviceState::Open;
}

void OneWire::close() noexcept
{
    state_ = DeviceState::Closed;
}

DeviceState OneWire::state() const noexcept
{
    return state_;
}

std::string_view OneWire::path() const noexcept
{
    return slavePath_;
}

std::size_t OneWire::read(std::span<std::byte> buf)
{
    const auto raw = readRaw();
    const auto n   = std::min(buf.size(), raw.size());
    if (n > 0) {
        std::memcpy(buf.data(), raw.data(), n);
    }
    return n;
}

std::size_t OneWire::write(std::span<const std::byte>)
{
    throw DeviceError(std::errc::operation_not_supported, "1-wire sysfs is read-only");
}

OneWireTemp OneWire::readTemperature()
{
    requireOpen();
    const auto text = readRaw();

    OneWireTemp out {};
    const char* yesPos = std::strstr(text.c_str(), "YES");
    const char* noPos  = std::strstr(text.c_str(), "NO");
    if (yesPos && (!noPos || yesPos < noPos)) {
        out.crcOk = true;
    }

    const char* tPos = std::strstr(text.c_str(), "t=");
    if (!tPos) {
        throw DeviceError(std::errc::protocol_error, "1-wire temperature parse");
    }

    tPos += 2;
    char*       end   = nullptr;
    const long  milli = std::strtol(tPos, &end, 10);
    if (end == tPos) {
        throw DeviceError(std::errc::protocol_error, "1-wire temperature value");
    }

    out.temperatureMilliC = static_cast<int32_t>(milli);
    out.temperatureC      = static_cast<float>(milli) / 1000.0f;
    return out;
}

std::string OneWire::readRaw()
{
    requireOpen();
    return readFile(slavePath_ + "/w1_slave");
}

const std::string& OneWire::slavePath() const noexcept
{
    return slavePath_;
}

}  // namespace lpio
