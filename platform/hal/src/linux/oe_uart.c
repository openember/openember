#define _GNU_SOURCE

#include "openember/hal/uart.h"

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

typedef struct {
    int fd;
    int inited;
} oe_uart_impl_t;

_Static_assert(sizeof(oe_uart_impl_t) <= sizeof(oe_uart_t), "oe_uart_t opaque too small");

static oe_uart_impl_t *impl(oe_uart_t *u)
{
    return (oe_uart_impl_t *)(void *)u;
}

static speed_t baud_to_flag(uint32_t baud)
{
    switch (baud) {
    case 9600:
        return B9600;
    case 19200:
        return B19200;
    case 38400:
        return B38400;
    case 57600:
        return B57600;
    case 115200:
        return B115200;
    case 230400:
        return B230400;
    case 460800:
        return B460800;
    case 921600:
        return B921600;
    default:
        return B115200;
    }
}

static oe_result_t apply_termios(int fd, const oe_uart_config_t *cfg)
{
    struct termios tio;

    if (tcgetattr(fd, &tio) != 0) {
        return OE_ERR_INTERNAL;
    }

    cfmakeraw(&tio);
    tio.c_cflag |= CLOCAL | CREAD;

    tio.c_cflag &= ~(CSIZE | PARENB | PARODD | CSTOPB);
    if (cfg->data_bits == 7) {
        tio.c_cflag |= CS7;
    } else {
        tio.c_cflag |= CS8;
    }

    if (cfg->stop_bits == 2) {
        tio.c_cflag |= CSTOPB;
    }

    switch (cfg->parity) {
    case OE_UART_PARITY_NONE:
        break;
    case OE_UART_PARITY_EVEN:
        tio.c_cflag |= PARENB;
        tio.c_cflag &= ~PARODD;
        break;
    case OE_UART_PARITY_ODD:
        tio.c_cflag |= PARENB | PARODD;
        break;
    default:
        return OE_ERR_INVALID_ARG;
    }

    cfsetispeed(&tio, baud_to_flag(cfg->baud_rate));
    cfsetospeed(&tio, baud_to_flag(cfg->baud_rate));

    if (tcsetattr(fd, TCSANOW, &tio) != 0) {
        return OE_ERR_INTERNAL;
    }
    return OE_OK;
}

oe_result_t oe_uart_open(oe_uart_t *u, const char *path, const oe_uart_config_t *cfg)
{
    int fd;
    oe_result_t r;

    if (!u || !path || !cfg) {
        return OE_ERR_INVALID_ARG;
    }

    if (cfg->data_bits != 7 && cfg->data_bits != 8) {
        return OE_ERR_INVALID_ARG;
    }
    if (cfg->stop_bits != 1 && cfg->stop_bits != 2) {
        return OE_ERR_INVALID_ARG;
    }

    memset(u, 0, sizeof(*u));
    fd = open(path, O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (fd < 0) {
        return OE_ERR_INTERNAL;
    }

    r = apply_termios(fd, cfg);
    if (r != OE_OK) {
        close(fd);
        memset(u, 0, sizeof(*u));
        return r;
    }

    {
        int fl = fcntl(fd, F_GETFL, 0);
        if (fl >= 0) {
            (void)fcntl(fd, F_SETFL, fl & ~O_NONBLOCK);
        }
    }

    impl(u)->fd = fd;
    impl(u)->inited = 1;
    return OE_OK;
}

oe_result_t oe_uart_close(oe_uart_t *u)
{
    int fd;

    if (!u) {
        return OE_ERR_INVALID_ARG;
    }

    fd = impl(u)->fd;
    memset(u, 0, sizeof(*u));
    if (fd > 0) {
        if (close(fd) != 0) {
            return OE_ERR_INTERNAL;
        }
    }
    return OE_OK;
}

oe_result_t oe_uart_read(oe_uart_t *u, void *buf, size_t len, size_t *out_read)
{
    ssize_t n;

    if (!u || (!buf && len > 0)) {
        return OE_ERR_INVALID_ARG;
    }

    if (!impl(u)->inited || impl(u)->fd < 0) {
        return OE_ERR_INVALID_ARG;
    }

    if (len == 0) {
        if (out_read) {
            *out_read = 0;
        }
        return OE_OK;
    }

    do {
        n = read(impl(u)->fd, buf, len);
    } while (n < 0 && errno == EINTR);

    if (n < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            if (out_read) {
                *out_read = 0;
            }
            return OE_ERR_AGAIN;
        }
        return OE_ERR_INTERNAL;
    }

    if (out_read) {
        *out_read = (size_t)n;
    }
    return OE_OK;
}

oe_result_t oe_uart_write(oe_uart_t *u, const void *buf, size_t len, size_t *out_written)
{
    ssize_t n;
    size_t total = 0;
    const uint8_t *p = (const uint8_t *)buf;

    if (!u || (!buf && len > 0)) {
        return OE_ERR_INVALID_ARG;
    }

    if (!impl(u)->inited || impl(u)->fd < 0) {
        return OE_ERR_INVALID_ARG;
    }

    while (total < len) {
        do {
            n = write(impl(u)->fd, p + total, len - total);
        } while (n < 0 && errno == EINTR);

        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                if (out_written) {
                    *out_written = total;
                }
                return total > 0 ? OE_OK : OE_ERR_AGAIN;
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

oe_result_t oe_uart_query_caps(oe_uart_caps_t *out_caps)
{
    uint32_t i;
    static const uint32_t rates[] = {9600, 19200, 38400, 57600, 115200, 230400, 460800, 921600};

    if (!out_caps) {
        return OE_ERR_INVALID_ARG;
    }

    out_caps->baud_rate_count = (uint32_t)(sizeof(rates) / sizeof(rates[0]));
    if (out_caps->baud_rate_count > OE_UART_CAPS_MAX_BAUD_RATES) {
        out_caps->baud_rate_count = OE_UART_CAPS_MAX_BAUD_RATES;
    }

    for (i = 0; i < out_caps->baud_rate_count; i++) {
        out_caps->baud_rates[i] = rates[i];
    }

    out_caps->parity_mask = OE_UART_PARITY_MASK_NONE | OE_UART_PARITY_MASK_EVEN | OE_UART_PARITY_MASK_ODD;
    out_caps->data_bits_min = 7;
    out_caps->data_bits_max = 8;
    out_caps->stop_bits_min = 1;
    out_caps->stop_bits_max = 2;
    return OE_OK;
}
