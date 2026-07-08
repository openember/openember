// src/GPIO.cpp
#include "lpio/GPIO.hpp"

#include <utility>
#include <cstring>
#include <cerrno>
#include <cassert>

#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <poll.h>
#include <linux/gpio.h>

namespace lpio {

namespace {

uint32_t toHandleFlags(const GPIOConfig& cfg)
{
    uint32_t flags = 0;

    switch (cfg.direction) {
    case GPIODirection::Input:
        flags |= GPIOHANDLE_REQUEST_INPUT;
        break;
    case GPIODirection::Output:
        flags |= GPIOHANDLE_REQUEST_OUTPUT;
        break;
    }

    switch (cfg.drive) {
    case GPIODrive::OpenDrain:
        flags |= GPIOHANDLE_REQUEST_OPEN_DRAIN;
        break;
    case GPIODrive::OpenSource:
        flags |= GPIOHANDLE_REQUEST_OPEN_SOURCE;
        break;
    case GPIODrive::PushPull:
        break;
    }

    if (cfg.activeLow) {
        flags |= GPIOHANDLE_REQUEST_ACTIVE_LOW;
    }

    (void)cfg.bias;  // bias not available on gpiohandle v1 uAPI
    return flags;
}

[[noreturn]] void throwErrno(std::string_view context)
{
    throw GPIOError(errno, context);
}

}  // namespace

GPIO::GPIO(std::string chipPath, GPIOConfig config)
    : chipPath_(std::move(chipPath))
    , config_(std::move(config))
{}

GPIO::~GPIO()
{
    close();
}

GPIO::GPIO(GPIO&& other) noexcept
    : chipPath_(std::move(other.chipPath_))
    , config_(std::move(other.config_))
    , state_(other.state_)
    , chipFd_(std::move(other.chipFd_))
    , lineFd_(std::move(other.lineFd_))
{
    other.state_  = DeviceState::Closed;
}

GPIO& GPIO::operator=(GPIO&& other) noexcept
{
    if (this != &other) {
        close();
        chipPath_ = std::move(other.chipPath_);
        config_   = std::move(other.config_);
        state_    = other.state_;
        chipFd_   = std::move(other.chipFd_);
        lineFd_   = std::move(other.lineFd_);
        other.state_  = DeviceState::Closed;
    }
    return *this;
}

void GPIO::open(OpenMode /*mode*/)
{
    if (state_ == DeviceState::Open) {
        return;
    }

    detail::UniqueFd chipFd(::open(chipPath_.c_str(), O_RDONLY | O_CLOEXEC));
    if (!chipFd) {
        throwErrno("GPIO::open — chip: " + chipPath_);
    }

    lineFd_ = requestLine(chipFd.get(), config_, chipPath_);
    chipFd_ = std::move(chipFd);

    state_ = DeviceState::Open;
}

void GPIO::close() noexcept
{
    releaseLine();

    chipFd_.reset();

    state_ = DeviceState::Closed;
}

DeviceState GPIO::state() const noexcept
{
    return state_;
}

std::string_view GPIO::path() const noexcept
{
    return chipPath_;
}

std::size_t GPIO::read(std::span<std::byte> /*buf*/)
{
    throw GPIOError(std::errc::operation_not_supported,
                    "GPIO does not support byte-stream read(); use get()");
}

std::size_t GPIO::write(std::span<const std::byte> /*buf*/)
{
    throw GPIOError(std::errc::operation_not_supported,
                    "GPIO does not support byte-stream write(); use set()");
}

GPIOValue GPIO::get() const
{
    requireOpen();

    gpiohandle_data data {};
    if (::ioctl(lineFd_.get(), GPIOHANDLE_GET_LINE_VALUES_IOCTL, &data) < 0) {
        throw GPIOError(errno, "GPIO::get — " + chipPath_);
    }

    return data.values[0] ? GPIOValue::High : GPIOValue::Low;
}

void GPIO::set(GPIOValue value)
{
    requireOpen();

    if (config_.direction != GPIODirection::Output) {
        throw GPIOError(std::errc::invalid_argument,
                        "GPIO::set called on Input pin: " + chipPath_);
    }

    gpiohandle_data data {};
    data.values[0] = (value == GPIOValue::High) ? 1 : 0;

    if (::ioctl(lineFd_.get(), GPIOHANDLE_SET_LINE_VALUES_IOCTL, &data) < 0) {
        throw GPIOError(errno, "GPIO::set — " + chipPath_);
    }
}

void GPIO::toggle()
{
    set(get() == GPIOValue::High ? GPIOValue::Low : GPIOValue::High);
}

GPIODirection GPIO::direction() const noexcept
{
    return config_.direction;
}

void GPIO::setDirection(GPIODirection dir, GPIOValue initValue)
{
    GPIOConfig next = config_;
    next.direction = dir;
    next.initValue = initValue;

    if (state_ == DeviceState::Open) {
        releaseLine();
        try {
            lineFd_ = requestLine(chipFd_.get(), next, chipPath_);
        } catch (...) {
            close();
            throw;
        }
    }

    config_ = std::move(next);
}

std::optional<GPIOValue> GPIO::waitForEdge(GPIOEdge edge,
                                           std::chrono::milliseconds timeout)
{
    requireOpen();

    if (config_.direction != GPIODirection::Input) {
        throw GPIOError(std::errc::invalid_argument,
                        "GPIO::waitForEdge called on Output pin: " + chipPath_);
    }

    uint32_t eventFlags = 0;
    switch (edge) {
    case GPIOEdge::Rising:
        eventFlags = GPIOEVENT_REQUEST_RISING_EDGE;
        break;
    case GPIOEdge::Falling:
        eventFlags = GPIOEVENT_REQUEST_FALLING_EDGE;
        break;
    case GPIOEdge::Both:
        eventFlags = GPIOEVENT_REQUEST_BOTH_EDGES;
        break;
    case GPIOEdge::None:
        throw GPIOError(std::errc::invalid_argument, "GPIO::waitForEdge GPIOEdge::None");
    }

    gpioevent_request ereq {};
    ereq.lineoffset   = config_.line;
    ereq.handleflags  = toHandleFlags(config_);
    ereq.eventflags   = eventFlags;
    std::strncpy(ereq.consumer_label, config_.consumer.c_str(),
                 sizeof(ereq.consumer_label) - 1);

    if (::ioctl(chipFd_.get(), GPIO_GET_LINEEVENT_IOCTL, &ereq) < 0) {
        throw GPIOError(errno, "GPIO::waitForEdge — " + chipPath_);
    }

    detail::UniqueFd eventFd(ereq.fd);

    pollfd pfd {};
    pfd.fd     = eventFd.get();
    pfd.events = POLLIN;

    const int ret = ::poll(&pfd, 1, static_cast<int>(timeout.count()));
    if (ret < 0) {
        throw GPIOError(errno, "GPIO::waitForEdge poll — " + chipPath_);
    }

    if (ret == 0) {
        return std::nullopt;
    }

    gpioevent_data evdata {};
    if (::read(eventFd.get(), &evdata, sizeof(evdata)) < 0) {
        throw GPIOError(errno, "GPIO::waitForEdge read — " + chipPath_);
    }

    const GPIOValue triggered =
        (evdata.id == GPIOEVENT_EVENT_RISING_EDGE) ? GPIOValue::High : GPIOValue::Low;
    return triggered;
}

const GPIOConfig& GPIO::config() const noexcept
{
    return config_;
}

detail::UniqueFd GPIO::requestLine(int chipFd, const GPIOConfig& config, std::string_view chipPath)
{
    assert(chipFd >= 0);

    gpiohandle_request req {};
    req.lineoffsets[0] = config.line;
    req.lines          = 1;
    req.flags          = toHandleFlags(config);

    if (config.direction == GPIODirection::Output) {
        req.default_values[0] = (config.initValue == GPIOValue::High) ? 1 : 0;
    }

    std::strncpy(req.consumer_label, config.consumer.c_str(),
                 sizeof(req.consumer_label) - 1);

    if (::ioctl(chipFd, GPIO_GET_LINEHANDLE_IOCTL, &req) < 0) {
        throwErrno("GPIO::requestLine — " + std::string(chipPath));
    }

    return detail::UniqueFd(req.fd);
}

void GPIO::releaseLine() noexcept
{
    lineFd_.reset();
}

GPIO::Builder::Builder(std::string chipPath) : chipPath_(std::move(chipPath)) {}

GPIO::Builder& GPIO::Builder::line(uint32_t line)
{
    cfg_.line = line;
    return *this;
}

GPIO::Builder& GPIO::Builder::direction(GPIODirection dir)
{
    cfg_.direction = dir;
    return *this;
}

GPIO::Builder& GPIO::Builder::initValue(GPIOValue value)
{
    cfg_.initValue = value;
    return *this;
}

GPIO::Builder& GPIO::Builder::bias(GPIOBias b)
{
    cfg_.bias = b;
    return *this;
}

GPIO::Builder& GPIO::Builder::drive(GPIODrive d)
{
    cfg_.drive = d;
    return *this;
}

GPIO::Builder& GPIO::Builder::activeLow(bool enable)
{
    cfg_.activeLow = enable;
    return *this;
}

GPIO::Builder& GPIO::Builder::consumer(std::string name)
{
    cfg_.consumer = std::move(name);
    return *this;
}

GPIO GPIO::Builder::build()
{
    return GPIO(chipPath_, cfg_);
}

GPIO GPIO::Builder::open(OpenMode mode)
{
    auto dev = build();
    dev.open(mode);
    return dev;
}

GPIO::Builder GPIO::on(std::string chipPath)
{
    return Builder(std::move(chipPath));
}

}  // namespace lpio
