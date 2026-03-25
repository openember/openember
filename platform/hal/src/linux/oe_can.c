#define _GNU_SOURCE

#include "openember/hal/can.h"

#include <errno.h>
#include <poll.h>
#include <string.h>
#include <unistd.h>

#include <net/if.h>
#include <sys/socket.h>

#include <linux/can.h>
#include <linux/can/raw.h>

typedef struct {
    int fd;
    int inited;
    int can_fd_enabled;
} oe_can_impl_t;

_Static_assert(sizeof(oe_can_impl_t) <= sizeof(oe_can_t), "oe_can_t opaque too small");

static oe_can_impl_t *impl(oe_can_t *c)
{
    return (oe_can_impl_t *)(void *)c;
}

oe_result_t oe_can_query_caps(oe_can_caps_t *out_caps)
{
    if (!out_caps) {
        return OE_ERR_INVALID_ARG;
    }

    out_caps->max_data_len_can = 8;
#ifdef CAN_RAW_FD_FRAMES
    out_caps->supports_can_fd = 1;
    out_caps->max_data_len_can_fd = 64;
#else
    out_caps->supports_can_fd = 0;
    out_caps->max_data_len_can_fd = 0;
#endif
    return OE_OK;
}

oe_result_t oe_can_open(oe_can_t *c, const char *ifname, const oe_can_config_t *cfg)
{
    oe_can_impl_t *p;
    int fd;
    unsigned ifindex;
    struct sockaddr_can addr;
    int enable_fd = 0;
    oe_can_caps_t caps;

    if (!c || !ifname || !cfg) {
        return OE_ERR_INVALID_ARG;
    }

    memset(c, 0, sizeof(*c));
    p = impl(c);
    p->fd = -1;

    if (oe_can_query_caps(&caps) != OE_OK) {
        return OE_ERR_INTERNAL;
    }
    if (cfg->enable_can_fd && caps.supports_can_fd == 0) {
        return OE_ERR_UNSUPPORTED;
    }

    fd = socket(PF_CAN, SOCK_RAW, CAN_RAW);
    if (fd < 0) {
        return OE_ERR_INTERNAL;
    }

    ifindex = if_nametoindex(ifname);
    if (ifindex == 0) {
        close(fd);
        return OE_ERR_INVALID_ARG;
    }

    memset(&addr, 0, sizeof(addr));
    addr.can_family = AF_CAN;
    addr.can_ifindex = ifindex;

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
        close(fd);
        return OE_ERR_INTERNAL;
    }

#ifdef CAN_RAW_FD_FRAMES
    if (cfg->enable_can_fd) {
        enable_fd = 1;
        if (setsockopt(fd, SOL_CAN_RAW, CAN_RAW_FD_FRAMES, &enable_fd, sizeof(enable_fd)) != 0) {
            close(fd);
            return OE_ERR_UNSUPPORTED;
        }
        p->can_fd_enabled = 1;
    }
#endif

    p->fd = fd;
    p->inited = 1;
    return OE_OK;
}

oe_result_t oe_can_close(oe_can_t *c)
{
    oe_can_impl_t *p;
    int fd;

    if (!c) {
        return OE_ERR_INVALID_ARG;
    }

    p = impl(c);
    fd = p->fd;
    memset(c, 0, sizeof(*c));

    if (fd >= 0) {
        if (close(fd) != 0) {
            return OE_ERR_INTERNAL;
        }
    }
    return OE_OK;
}

oe_result_t oe_can_send_frame(oe_can_t *c, const oe_can_frame_t *frame)
{
    oe_can_impl_t *p;
    struct can_frame cf;
    struct canfd_frame cfd;
    canid_t can_id;

    if (!c || !frame) {
        return OE_ERR_INVALID_ARG;
    }

    p = impl(c);
    if (!p->inited || p->fd < 0) {
        return OE_ERR_INVALID_ARG;
    }

    can_id = frame->can_id;
    if (frame->is_extended) {
        can_id |= CAN_EFF_FLAG;
    }
    if (frame->is_fd) {
        if (!p->can_fd_enabled) {
            return OE_ERR_UNSUPPORTED;
        }
        if (frame->data_len > 64) {
            return OE_ERR_INVALID_ARG;
        }

        memset(&cfd, 0, sizeof(cfd));
        cfd.can_id = can_id;
        cfd.len = frame->data_len;
        cfd.flags = 0;
        if (frame->data_len > 0) {
            memcpy(cfd.data, frame->data, frame->data_len);
        }
        if (write(p->fd, &cfd, sizeof(cfd)) != (ssize_t)sizeof(cfd)) {
            return OE_ERR_INTERNAL;
        }
        return OE_OK;
    }

    if (frame->data_len > 8) {
        return OE_ERR_INVALID_ARG;
    }

    memset(&cf, 0, sizeof(cf));
    cf.can_id = can_id;
    cf.can_dlc = frame->data_len;
    if (frame->data_len > 0) {
        memcpy(cf.data, frame->data, frame->data_len);
    }
    if (write(p->fd, &cf, sizeof(cf)) != (ssize_t)sizeof(cf)) {
        return OE_ERR_INTERNAL;
    }
    return OE_OK;
}

oe_result_t oe_can_recv_frame(oe_can_t *c, oe_can_frame_t *out_frame, int timeout_ms)
{
    oe_can_impl_t *p;
    struct pollfd fds[1];
    ssize_t n;

    if (!c || !out_frame) {
        return OE_ERR_INVALID_ARG;
    }

    p = impl(c);
    if (!p->inited || p->fd < 0) {
        return OE_ERR_INVALID_ARG;
    }

    fds[0].fd = p->fd;
    fds[0].events = POLLIN;
    fds[0].revents = 0;

    /* poll: timeout_ms <0 means infinite */
    int rc = poll(fds, 1, timeout_ms);
    if (rc < 0) {
        if (errno == EINTR) {
            return OE_ERR_AGAIN;
        }
        return OE_ERR_INTERNAL;
    }
    if (rc == 0) {
        return OE_ERR_TIMEOUT;
    }

    if (p->can_fd_enabled) {
        struct canfd_frame cfd;
        memset(&cfd, 0, sizeof(cfd));
        n = read(p->fd, &cfd, sizeof(cfd));
        if (n == (ssize_t)sizeof(cfd)) {
            out_frame->can_id = (uint32_t)(cfd.can_id & CAN_EFF_MASK);
            out_frame->is_extended = ((cfd.can_id & CAN_EFF_FLAG) != 0) ? 1 : 0;
            out_frame->is_fd = 1;
            out_frame->data_len = cfd.len;
            if (out_frame->data_len > 0) {
                memcpy(out_frame->data, cfd.data, out_frame->data_len);
            }
            return OE_OK;
        }
        /* Kernel may still deliver classical frame even with FD enabled. */
    }

    {
        struct can_frame cf;
        memset(&cf, 0, sizeof(cf));
        n = read(p->fd, &cf, sizeof(cf));
        if (n != (ssize_t)sizeof(cf)) {
            return OE_ERR_INTERNAL;
        }
        out_frame->can_id = (uint32_t)(cf.can_id & CAN_EFF_MASK);
        out_frame->is_extended = ((cf.can_id & CAN_EFF_FLAG) != 0) ? 1 : 0;
        out_frame->is_fd = 0;
        out_frame->data_len = cf.can_dlc;
        if (out_frame->data_len > 0) {
            memcpy(out_frame->data, cf.data, out_frame->data_len);
        }
        return OE_OK;
    }
}

