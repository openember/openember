#define _GNU_SOURCE

#include "openember/hal/sbus.h"
#include "openember/hal/uart.h"
#include "openember/osal/time.h"

#include <errno.h>
#include <string.h>

typedef struct {
    oe_uart_t uart;
    int inited;
    uint32_t baud_rate;
    uint8_t nonblocking;
} oe_sbus_impl_t;

_Static_assert(sizeof(oe_sbus_impl_t) <= sizeof(((oe_sbus_t *)0)->opaque),
               "oe_sbus_t opaque too small");

static oe_sbus_impl_t *impl(oe_sbus_t *s)
{
    return (oe_sbus_impl_t *)(void *)s->opaque;
}

oe_result_t oe_sbus_query_caps(oe_sbus_caps_t *out_caps)
{
    if (!out_caps) {
        return OE_ERR_INVALID_ARG;
    }
    out_caps->channels_count = 16;
    out_caps->switches_count = 2;
    out_caps->frame_len_bytes = 25;
    out_caps->baud_rate = 100000;
    return OE_OK;
}

oe_result_t oe_sbus_open(oe_sbus_t *s, const char *uart_path, const oe_sbus_config_t *cfg)
{
    oe_sbus_impl_t *p;
    oe_uart_config_t ucfg;

    if (!s || !uart_path || !cfg) {
        return OE_ERR_INVALID_ARG;
    }

    memset(s, 0, sizeof(*s));
    p = impl(s);
    p->inited = 0;

    p->baud_rate = (cfg->baud_rate != 0) ? cfg->baud_rate : 100000;
    p->nonblocking = (cfg->nonblocking != 0) ? 1 : 0;

    memset(&ucfg, 0, sizeof(ucfg));
    ucfg.baud_rate = p->baud_rate;
    ucfg.data_bits = 8;
    ucfg.stop_bits = 2;
    ucfg.parity = OE_UART_PARITY_EVEN;
    ucfg.nonblocking = p->nonblocking;

    {
        oe_result_t r = oe_uart_open(&p->uart, uart_path, &ucfg);
        if (r != OE_OK) {
            return r;
        }
    }

    p->inited = 1;
    return OE_OK;
}

oe_result_t oe_sbus_close(oe_sbus_t *s)
{
    oe_sbus_impl_t *p;

    if (!s) {
        return OE_ERR_INVALID_ARG;
    }
    p = impl(s);
    oe_uart_close(&p->uart);
    memset(s, 0, sizeof(*s));
    return OE_OK;
}

static oe_result_t decode_sbus_frame(const uint8_t raw[25], oe_sbus_frame_t *out_frame)
{
    uint8_t flags;
    int i;

    if (!raw || !out_frame) {
        return OE_ERR_INVALID_ARG;
    }
    memset(out_frame, 0, sizeof(*out_frame));
    memcpy(out_frame->raw, raw, 25);

    /* Channels: packed 16 x 11-bit values across raw[1..22] */
    for (i = 0; i < 16; i++) {
        int bit_index = i * 11;
        int byte_index = 1 + (bit_index / 8);
        int bit_offset = bit_index % 8;
        uint16_t v = (uint16_t)(raw[byte_index] >> bit_offset) |
                      (uint16_t)(raw[byte_index + 1] << (8 - bit_offset));
        out_frame->channels[i] = (uint16_t)(v & 0x07FFu);
    }

    flags = raw[23];
    out_frame->frame_lost = (flags & 0x04u) ? 1u : 0u;
    out_frame->failsafe = (flags & 0x08u) ? 1u : 0u;
    out_frame->switches[0] = (flags & 0x10u) ? 1u : 0u; /* ch17 */
    out_frame->switches[1] = (flags & 0x20u) ? 1u : 0u; /* ch18 */
    return OE_OK;
}

oe_result_t oe_sbus_recv_frame(oe_sbus_t *s, oe_sbus_frame_t *out_frame, int timeout_ms)
{
    oe_sbus_impl_t *p;
    uint8_t raw[25];
    size_t got = 0;
    uint64_t start_ns = 0;
    uint64_t deadline_ns = 0;
    oe_result_t r;

    if (!s || !out_frame) {
        return OE_ERR_INVALID_ARG;
    }

    p = impl(s);
    if (!p->inited) {
        return OE_ERR_INVALID_ARG;
    }

    memset(raw, 0, sizeof(raw));

    if (timeout_ms > 0) {
        uint64_t now_ns = 0;
        (void)oe_clock_monotonic_ns(&now_ns);
        start_ns = now_ns;
        deadline_ns = start_ns + ((uint64_t)timeout_ms * 1000000ull);
    }

    while (got < sizeof(raw)) {
        size_t want = sizeof(raw) - got;
        size_t rd = 0;
        r = oe_uart_read(&p->uart, raw + got, want, &rd);
        if (r == OE_OK) {
            if (rd == 0) {
                /* Nonblocking read may return OK with 0 bytes (no progress). */
                if (timeout_ms == 0) {
                    return OE_ERR_TIMEOUT;
                }
                if (timeout_ms > 0) {
                    uint64_t now_ns = 0;
                    (void)oe_clock_monotonic_ns(&now_ns);
                    if (now_ns >= deadline_ns) {
                        return OE_ERR_TIMEOUT;
                    }
                }
                (void)oe_sleep_ms(1);
                continue;
            }
            got += rd;
            continue;
        }
        if (r == OE_ERR_AGAIN) {
            if (timeout_ms == 0) {
                return OE_ERR_TIMEOUT;
            }
            if (timeout_ms > 0) {
                uint64_t now_ns = 0;
                (void)oe_clock_monotonic_ns(&now_ns);
                if (now_ns >= deadline_ns) {
                    return OE_ERR_TIMEOUT;
                }
            }
            (void)oe_sleep_ms(1);
            continue;
        }
        return r;
    }

    /* Basic framing checks: 0x0F header, 0x00 footer */
    if (raw[0] != 0x0Fu || raw[24] != 0x00u) {
        return OE_ERR_INTERNAL;
    }

    return decode_sbus_frame(raw, out_frame);
}

