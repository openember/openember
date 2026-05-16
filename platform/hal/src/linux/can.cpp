#define _GNU_SOURCE

#include "openember/hal/can.hpp"

#include <cerrno>
#include <cstring>
#include <linux/can.h>
#include <linux/can/raw.h>
#include <net/if.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

namespace openember::hal {

struct Can::Impl {
    int fd = -1;
    bool inited = false;
    int can_fd_enabled = 0;
};

Can::Can() : impl_(std::make_unique<Impl>()) {}

Can::~Can()
{
    (void)close();
}

Result Can::query_caps(CanCaps* out_caps)
{
    if (!out_caps) {
        return osal::kErrInvalidArg;
    }

    out_caps->max_data_len_can = 8;
#ifdef CAN_RAW_FD_FRAMES
    out_caps->supports_can_fd = 1;
    out_caps->max_data_len_can_fd = 64;
#else
    out_caps->supports_can_fd = 0;
    out_caps->max_data_len_can_fd = 0;
#endif
    return osal::kOk;
}

Result Can::open(const std::string& ifname, const CanConfig& cfg)
{
    if (ifname.empty()) {
        return osal::kErrInvalidArg;
    }

    (void)close();

    CanCaps caps {};
    if (query_caps(&caps) != osal::kOk) {
        return osal::kErrInternal;
    }
    if (cfg.enable_can_fd && caps.supports_can_fd == 0) {
        return osal::kErrUnsupported;
    }

    const int fd = socket(PF_CAN, SOCK_RAW, CAN_RAW);
    if (fd < 0) {
        return osal::kErrInternal;
    }

    const unsigned ifindex = if_nametoindex(ifname.c_str());
    if (ifindex == 0) {
        ::close(fd);
        return osal::kErrInvalidArg;
    }

    sockaddr_can addr {};
    addr.can_family = AF_CAN;
    addr.can_ifindex = ifindex;

    if (bind(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
        ::close(fd);
        return osal::kErrInternal;
    }

#ifdef CAN_RAW_FD_FRAMES
    if (cfg.enable_can_fd) {
        int enable_fd = 1;
        if (setsockopt(fd, SOL_CAN_RAW, CAN_RAW_FD_FRAMES, &enable_fd, sizeof(enable_fd)) != 0) {
            ::close(fd);
            return osal::kErrUnsupported;
        }
        impl_->can_fd_enabled = 1;
    }
#else
    (void)cfg;
#endif

    impl_->fd = fd;
    impl_->inited = true;
    return osal::kOk;
}

Result Can::close()
{
    const int fd = impl_->fd;
    impl_->fd = -1;
    impl_->inited = false;
    impl_->can_fd_enabled = 0;

    if (fd >= 0) {
        return ::close(fd) == 0 ? osal::kOk : osal::kErrInternal;
    }
    return osal::kOk;
}

Result Can::send_frame(const CanFrame& frame)
{
    if (!impl_->inited || impl_->fd < 0) {
        return osal::kErrInvalidArg;
    }

    canid_t can_id = frame.can_id;
    if (frame.is_extended) {
        can_id |= CAN_EFF_FLAG;
    }

    if (frame.is_fd) {
        if (!impl_->can_fd_enabled) {
            return osal::kErrUnsupported;
        }
        if (frame.data_len > 64) {
            return osal::kErrInvalidArg;
        }

        canfd_frame cfd {};
        cfd.can_id = can_id;
        cfd.len = frame.data_len;
        if (frame.data_len > 0) {
            std::memcpy(cfd.data, frame.data, frame.data_len);
        }
        if (::write(impl_->fd, &cfd, sizeof(cfd)) != static_cast<ssize_t>(sizeof(cfd))) {
            return osal::kErrInternal;
        }
        return osal::kOk;
    }

    if (frame.data_len > 8) {
        return osal::kErrInvalidArg;
    }

    can_frame cf {};
    cf.can_id = can_id;
    cf.can_dlc = frame.data_len;
    if (frame.data_len > 0) {
        std::memcpy(cf.data, frame.data, frame.data_len);
    }
    if (::write(impl_->fd, &cf, sizeof(cf)) != static_cast<ssize_t>(sizeof(cf))) {
        return osal::kErrInternal;
    }
    return osal::kOk;
}

Result Can::recv_frame(CanFrame* out_frame, int timeout_ms)
{
    if (!out_frame) {
        return osal::kErrInvalidArg;
    }

    if (!impl_->inited || impl_->fd < 0) {
        return osal::kErrInvalidArg;
    }

    pollfd fds[1] {};
    fds[0].fd = impl_->fd;
    fds[0].events = POLLIN;

    const int rc = poll(fds, 1, timeout_ms);
    if (rc < 0) {
        return errno == EINTR ? osal::kErrAgain : osal::kErrInternal;
    }
    if (rc == 0) {
        return osal::kErrTimeout;
    }

    union {
        can_frame cf;
        canfd_frame cfd;
    } u {};

    const ssize_t n = ::read(impl_->fd, &u, sizeof(u));
    if (n == static_cast<ssize_t>(sizeof(u.cfd))) {
        out_frame->can_id = static_cast<uint32_t>(u.cfd.can_id & CAN_EFF_MASK);
        out_frame->is_extended = (u.cfd.can_id & CAN_EFF_FLAG) != 0 ? 1 : 0;
        out_frame->is_fd = 1;
        out_frame->data_len = u.cfd.len;
        if (out_frame->data_len > 0) {
            std::memcpy(out_frame->data, u.cfd.data, out_frame->data_len);
        }
        return osal::kOk;
    }
    if (n == static_cast<ssize_t>(sizeof(u.cf))) {
        out_frame->can_id = static_cast<uint32_t>(u.cf.can_id & CAN_EFF_MASK);
        out_frame->is_extended = (u.cf.can_id & CAN_EFF_FLAG) != 0 ? 1 : 0;
        out_frame->is_fd = 0;
        out_frame->data_len = u.cf.can_dlc;
        if (out_frame->data_len > 0) {
            std::memcpy(out_frame->data, u.cf.data, out_frame->data_len);
        }
        return osal::kOk;
    }

    return osal::kErrInternal;
}

}  // namespace openember::hal
