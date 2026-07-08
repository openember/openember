#include "lpio/CANBus.hpp"

#include <cerrno>
#include <cstring>
#include <linux/can.h>
#include <linux/can/raw.h>
#include <net/if.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

namespace lpio {

namespace {

[[noreturn]] void throwErrno(std::string_view context)
{
    throw DeviceError(errno, context);
}

}  // namespace

CANBus::Builder::Builder(std::string ifname) : ifname_(std::move(ifname)) {}

CANBus::Builder& CANBus::Builder::canFd(bool enable)
{
    cfg_.enableCanFd = enable;
    return *this;
}

CANBus CANBus::Builder::build()
{
    return CANBus(ifname_, cfg_);
}

CANBus CANBus::Builder::buildAndOpen(OpenMode mode)
{
    auto dev = build();
    dev.open(mode);
    return dev;
}

CANBus::Builder CANBus::on(std::string ifname)
{
    return Builder(std::move(ifname));
}

CANBus::CANBus(std::string ifname, CANBusConfig config)
    : ifname_(std::move(ifname))
    , config_(config)
{}

CANBus::~CANBus()
{
    close();
}

CANBus::CANBus(CANBus&& other) noexcept
    : detail::FdDeviceBase(std::move(other))
    , ifname_(std::move(other.ifname_))
    , config_(other.config_)
    , state_(other.state_)
    , canFdEnabled_(other.canFdEnabled_)
{
    other.state_ = DeviceState::Closed;
    other.canFdEnabled_ = false;
}

CANBus& CANBus::operator=(CANBus&& other) noexcept
{
    if (this != &other) {
        close();
        detail::FdDeviceBase::operator=(std::move(other));
        ifname_       = std::move(other.ifname_);
        config_       = other.config_;
        state_        = other.state_;
        canFdEnabled_ = other.canFdEnabled_;
        other.state_  = DeviceState::Closed;
        other.canFdEnabled_ = false;
    }
    return *this;
}

void CANBus::open(OpenMode mode)
{
    (void)mode;
    if (state_ == DeviceState::Open) {
        return;
    }

    if (ifname_.empty()) {
        throw DeviceError(std::errc::invalid_argument, "CAN interface name is empty");
    }

    detail::UniqueFd fd(socket(PF_CAN, SOCK_RAW, CAN_RAW));
    if (!fd) {
        throwErrno("socket(PF_CAN)");
    }

    const unsigned ifindex = if_nametoindex(ifname_.c_str());
    if (ifindex == 0) {
        throw DeviceError(std::errc::invalid_argument, ifname_);
    }

    sockaddr_can addr {};
    addr.can_family  = AF_CAN;
    addr.can_ifindex = ifindex;

    if (bind(fd.get(), reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
        throwErrno("bind CAN");
    }

#ifdef CAN_RAW_FD_FRAMES
    if (config_.enableCanFd) {
        int enableFd = 1;
        if (setsockopt(fd.get(), SOL_CAN_RAW, CAN_RAW_FD_FRAMES, &enableFd, sizeof(enableFd)) != 0) {
            throw DeviceError(std::errc::not_supported, "CAN FD");
        }
        canFdEnabled_ = true;
    }
#else
    if (config_.enableCanFd) {
        throw DeviceError(std::errc::not_supported, "CAN FD");
    }
#endif

    setFd(std::move(fd));
    state_ = DeviceState::Open;
}

void CANBus::close() noexcept
{
    closeFd();
    canFdEnabled_ = false;
    state_        = DeviceState::Closed;
}

DeviceState CANBus::state() const noexcept
{
    return state_;
}

std::string_view CANBus::path() const noexcept
{
    return ifname_;
}

std::size_t CANBus::read(std::span<std::byte>)
{
    throw DeviceError(std::errc::operation_not_supported, "use recvFrame()");
}

std::size_t CANBus::write(std::span<const std::byte>)
{
    throw DeviceError(std::errc::operation_not_supported, "use sendFrame()");
}

void CANBus::sendFrame(const CANFrame& frame)
{
    requireOpen();

    canid_t canId = frame.canId;
    if (frame.isExtended) {
        canId |= CAN_EFF_FLAG;
    }

    if (frame.isFd) {
        if (!canFdEnabled_) {
            throw DeviceError(std::errc::not_supported, "CAN FD not enabled");
        }
        canfd_frame cfd {};
        cfd.can_id = canId;
        cfd.len    = frame.dataLen;
        if (frame.dataLen > 0) {
            std::memcpy(cfd.data, frame.data.data(), frame.dataLen);
        }
        if (::write(fd(), &cfd, sizeof(cfd)) != static_cast<ssize_t>(sizeof(cfd))) {
            throwErrno("CAN FD write");
        }
        return;
    }

    if (frame.dataLen > 8) {
        throw DeviceError(std::errc::invalid_argument, "classic CAN data len");
    }

    can_frame cf {};
    cf.can_id  = canId;
    cf.can_dlc = frame.dataLen;
    if (frame.dataLen > 0) {
        std::memcpy(cf.data, frame.data.data(), frame.dataLen);
    }
    if (::write(fd(), &cf, sizeof(cf)) != static_cast<ssize_t>(sizeof(cf))) {
        throwErrno("CAN write");
    }
}

CANFrame CANBus::recvFrame(std::chrono::milliseconds timeout)
{
    requireOpen();

    pollfd fds[1] {};
    fds[0].fd     = fd();
    fds[0].events = POLLIN;

    const int rc = poll(fds, 1, static_cast<int>(timeout.count()));
    if (rc < 0) {
        throwErrno("CAN poll");
    }
    if (rc == 0) {
        throw DeviceTimeoutError(ifname_);
    }

    union {
        can_frame cf;
        canfd_frame cfd;
    } u {};

    const ssize_t n = ::read(fd(), &u, sizeof(u));
    CANFrame out {};

    if (n == static_cast<ssize_t>(sizeof(u.cfd))) {
        out.canId      = static_cast<uint32_t>(u.cfd.can_id & CAN_EFF_MASK);
        out.isExtended = (u.cfd.can_id & CAN_EFF_FLAG) != 0;
        out.isFd       = true;
        out.dataLen    = u.cfd.len;
        if (out.dataLen > 0) {
            std::memcpy(out.data.data(), u.cfd.data, out.dataLen);
        }
        return out;
    }

    if (n == static_cast<ssize_t>(sizeof(u.cf))) {
        out.canId      = static_cast<uint32_t>(u.cf.can_id & CAN_EFF_MASK);
        out.isExtended = (u.cf.can_id & CAN_EFF_FLAG) != 0;
        out.isFd       = false;
        out.dataLen    = u.cf.can_dlc;
        if (out.dataLen > 0) {
            std::memcpy(out.data.data(), u.cf.data, out.dataLen);
        }
        return out;
    }

    throwErrno("CAN read");
}

const CANBusConfig& CANBus::config() const noexcept
{
    return config_;
}

}  // namespace lpio
