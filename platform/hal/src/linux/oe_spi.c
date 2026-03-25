#define _GNU_SOURCE

#include "openember/hal/spi.h"

#include <errno.h>
#include <fcntl.h>
#include <linux/spi/spidev.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

typedef struct {
    int fd;
    int inited;
    oe_spi_config_t cfg;
} oe_spi_impl_t;

_Static_assert(sizeof(oe_spi_impl_t) <= sizeof(oe_spi_t), "oe_spi_t opaque too small");

static oe_spi_impl_t *impl(oe_spi_t *s)
{
    return (oe_spi_impl_t *)(void *)s;
}

oe_result_t oe_spi_query_caps(oe_spi_caps_t *out_caps)
{
    if (!out_caps) {
        return OE_ERR_INVALID_ARG;
    }
    out_caps->supports_full_duplex = 1;
    out_caps->supports_mode_count = 4; /* SPI mode 0..3 */
    out_caps->min_bits_per_word = 8;
    out_caps->max_bits_per_word = 32;
    return OE_OK;
}

oe_result_t oe_spi_open(oe_spi_t *s, const char *dev_path, const oe_spi_config_t *cfg)
{
    int fd;
    oe_spi_impl_t *p;

    if (!s || !dev_path || !cfg) {
        return OE_ERR_INVALID_ARG;
    }

    memset(s, 0, sizeof(*s));
    p = impl(s);
    p->fd = -1;

    if (cfg->bits_per_word == 0) {
        return OE_ERR_INVALID_ARG;
    }

    fd = open(dev_path, O_RDWR);
    if (fd < 0) {
        return OE_ERR_INTERNAL;
    }

    /* Configure spidev. Ignore errors on some platforms? Here we fail on ioctl errors. */
    if (ioctl(fd, SPI_IOC_WR_MODE, &cfg->mode) < 0) {
        close(fd);
        return OE_ERR_UNSUPPORTED;
    }
    if (ioctl(fd, SPI_IOC_WR_BITS_PER_WORD, &cfg->bits_per_word) < 0) {
        close(fd);
        return OE_ERR_UNSUPPORTED;
    }
    if (ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &cfg->speed_hz) < 0) {
        close(fd);
        return OE_ERR_UNSUPPORTED;
    }

    p->fd = fd;
    p->inited = 1;
    p->cfg = *cfg;
    return OE_OK;
}

oe_result_t oe_spi_close(oe_spi_t *s)
{
    oe_spi_impl_t *p;
    int fd;

    if (!s) {
        return OE_ERR_INVALID_ARG;
    }

    p = impl(s);
    fd = p->fd;
    memset(s, 0, sizeof(*s));

    if (fd >= 0) {
        if (close(fd) != 0) {
            return OE_ERR_INTERNAL;
        }
    }
    return OE_OK;
}

oe_result_t oe_spi_transfer(oe_spi_t *s,
                             const void *tx,
                             void *rx,
                             size_t len,
                             size_t *out_transferred)
{
    oe_spi_impl_t *p;
    struct spi_ioc_transfer tr;
    const uint8_t *txp = (const uint8_t *)tx;
    uint8_t *rxp = (uint8_t *)rx;
    uint8_t *zero_tx = NULL;
    int rc;

    if (!s) {
        return OE_ERR_INVALID_ARG;
    }
    if (len == 0) {
        if (out_transferred) {
            *out_transferred = 0;
        }
        return OE_OK;
    }

    p = impl(s);
    if (!p->inited || p->fd < 0) {
        return OE_ERR_INVALID_ARG;
    }

    if (!txp) {
        zero_tx = (uint8_t *)calloc(len, 1);
        if (!zero_tx) {
            return OE_ERR_NOMEM;
        }
        txp = zero_tx;
    }

    memset(&tr, 0, sizeof(tr));
    tr.tx_buf = (uintptr_t)txp;
    tr.rx_buf = (uintptr_t)rxp;
    tr.len = (uint32_t)len;
    tr.delay_usecs = p->cfg.delay_usecs;
    tr.speed_hz = p->cfg.speed_hz;
    tr.bits_per_word = p->cfg.bits_per_word;

    rc = ioctl(p->fd, SPI_IOC_MESSAGE(1), &tr);
    if (rc < 0) {
        free(zero_tx);
        return OE_ERR_INTERNAL;
    }

    if (out_transferred) {
        *out_transferred = len;
    }

    free(zero_tx);
    return OE_OK;
}

