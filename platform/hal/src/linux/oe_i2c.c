#define _GNU_SOURCE

#include "openember/hal/i2c.h"

#include <errno.h>
#include <fcntl.h>
#include <linux/i2c-dev.h>
#include <linux/i2c.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>

typedef struct {
    int fd;
    uint8_t addr;
    int inited;
    uint32_t bus_num;
} oe_i2c_impl_t;

_Static_assert(sizeof(oe_i2c_impl_t) <= sizeof(oe_i2c_t), "oe_i2c_t opaque too small");

static oe_i2c_impl_t *impl(oe_i2c_t *i2c)
{
    return (oe_i2c_impl_t *)(void *)i2c;
}

oe_result_t oe_i2c_query_caps(oe_i2c_caps_t *out_caps)
{
    if (!out_caps) {
        return OE_ERR_INVALID_ARG;
    }
    out_caps->max_write_bytes = 0; /* unknown/unlimited */
    out_caps->max_read_bytes = 0;  /* unknown/unlimited */
    out_caps->supports_write_read = 1;
    return OE_OK;
}

oe_result_t oe_i2c_open(oe_i2c_t *i2c, uint32_t bus_num, uint8_t addr_7bit)
{
    int fd;
    char path[64];
    oe_i2c_impl_t *p;
    int rc;

    if (!i2c) {
        return OE_ERR_INVALID_ARG;
    }

    memset(i2c, 0, sizeof(*i2c));
    p = impl(i2c);
    p->bus_num = bus_num;
    p->addr = addr_7bit;

    snprintf(path, sizeof(path), "/dev/i2c-%u", bus_num);
    fd = open(path, O_RDWR);
    if (fd < 0) {
        return OE_ERR_INTERNAL;
    }

    rc = ioctl(fd, I2C_SLAVE, (int)addr_7bit);
    if (rc < 0) {
        close(fd);
        return OE_ERR_UNSUPPORTED;
    }

    p->fd = fd;
    p->inited = 1;
    return OE_OK;
}

oe_result_t oe_i2c_close(oe_i2c_t *i2c)
{
    oe_i2c_impl_t *p;
    int fd;

    if (!i2c) {
        return OE_ERR_INVALID_ARG;
    }

    p = impl(i2c);
    fd = p->fd;
    memset(i2c, 0, sizeof(*i2c));

    if (fd > 0) {
        if (close(fd) != 0) {
            return OE_ERR_INTERNAL;
        }
    }
    return OE_OK;
}

oe_result_t oe_i2c_write(oe_i2c_t *i2c, const uint8_t *data, size_t len, size_t *out_written)
{
    oe_i2c_impl_t *p;
    size_t total = 0;
    ssize_t n;

    if (!i2c || (!data && len > 0)) {
        return OE_ERR_INVALID_ARG;
    }

    p = impl(i2c);
    if (!p->inited || p->fd < 0) {
        return OE_ERR_INVALID_ARG;
    }

    while (total < len) {
        n = write(p->fd, data + total, len - total);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            return OE_ERR_INTERNAL;
        }
        total += (size_t)n;
    }

    if (out_written) {
        *out_written = total;
    }
    return OE_OK;
}

oe_result_t oe_i2c_read(oe_i2c_t *i2c, uint8_t *data, size_t len, size_t *out_read)
{
    oe_i2c_impl_t *p;
    size_t total = 0;
    ssize_t n;

    if (!i2c || (!data && len > 0)) {
        return OE_ERR_INVALID_ARG;
    }

    p = impl(i2c);
    if (!p->inited || p->fd < 0) {
        return OE_ERR_INVALID_ARG;
    }

    while (total < len) {
        n = read(p->fd, data + total, len - total);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            return OE_ERR_INTERNAL;
        }
        if (n == 0) {
            break;
        }
        total += (size_t)n;
    }

    if (out_read) {
        *out_read = total;
    }
    return (total == len) ? OE_OK : OE_ERR_INTERNAL;
}

oe_result_t oe_i2c_write_read(oe_i2c_t *i2c,
                                const uint8_t *wbuf,
                                size_t wlen,
                                uint8_t *rbuf,
                                size_t rlen)
{
    oe_i2c_impl_t *p;
    struct i2c_rdwr_ioctl_data rdwr;
    struct i2c_msg msgs[2];
    int rc;

    if (!i2c || (!wbuf && wlen > 0) || (!rbuf && rlen > 0)) {
        return OE_ERR_INVALID_ARG;
    }

    p = impl(i2c);
    if (!p->inited || p->fd < 0) {
        return OE_ERR_INVALID_ARG;
    }

    memset(&rdwr, 0, sizeof(rdwr));
    memset(&msgs, 0, sizeof(msgs));

    /* write */
    msgs[0].addr = p->addr;
    msgs[0].flags = 0;
    msgs[0].len = (uint16_t)wlen;
    msgs[0].buf = (uint8_t *)wbuf;

    /* read */
    msgs[1].addr = p->addr;
    msgs[1].flags = I2C_M_RD;
    msgs[1].len = (uint16_t)rlen;
    msgs[1].buf = rbuf;

    rdwr.msgs = msgs;
    rdwr.nmsgs = (wlen > 0) ? 2 : 1;
    if (wlen == 0) {
        rdwr.msgs = &msgs[1];
        rdwr.nmsgs = 1;
    }

    rc = ioctl(p->fd, I2C_RDWR, &rdwr);
    if (rc < 0) {
        return OE_ERR_INTERNAL;
    }
    return OE_OK;
}

