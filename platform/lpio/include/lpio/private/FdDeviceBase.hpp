#pragma once

#include "lpio/DeviceBase.hpp"
#include "lpio/private/UniqueFd.hpp"

#include <utility>

namespace lpio::detail {

class FdDeviceBase : public DeviceBase {
public:
    FdDeviceBase(const FdDeviceBase&)            = delete;
    FdDeviceBase& operator=(const FdDeviceBase&) = delete;

protected:
    FdDeviceBase() = default;
    ~FdDeviceBase() override = default;

    FdDeviceBase(FdDeviceBase&&) noexcept            = default;
    FdDeviceBase& operator=(FdDeviceBase&&) noexcept = default;

    int fd() const noexcept { return fd_.get(); }
    bool hasFd() const noexcept { return static_cast<bool>(fd_); }

    void setFd(UniqueFd fd) noexcept
    {
        fd_ = std::move(fd);
    }

    void closeFd() noexcept
    {
        fd_.reset();
    }

private:
    UniqueFd fd_;
};

}  // namespace lpio::detail
