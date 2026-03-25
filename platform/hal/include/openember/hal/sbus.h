/* OpenEmber HAL — Protocol-over-UART: SBUS (C ABI)
 *
 * SBUS is a digital serial protocol used by Futaba/Futaba-like RC systems.
 * HAL provides a small framing + channel extraction layer on top of oe_uart.
 */
#ifndef OPENEMBER_HAL_SBUS_H_
#define OPENEMBER_HAL_SBUS_H_

#include "openember/osal/types.h"

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct oe_sbus {
    uint8_t opaque[256];
} oe_sbus_t;

typedef struct oe_sbus_caps {
    uint32_t channels_count; /* analog channels: 16 */
    uint32_t switches_count; /* boolean channels: 2 */
    uint32_t frame_len_bytes; /* 25 */
    uint32_t baud_rate; /* default 100000 */
} oe_sbus_caps_t;

typedef struct oe_sbus_frame {
    uint16_t channels[16]; /* 11-bit values packed -> 0..2047 */
    uint8_t switches[2];   /* each 0/1 */

    uint8_t frame_lost; /* from SBUS status bits */
    uint8_t failsafe;   /* from SBUS status bits */

    uint8_t raw[25]; /* full SBUS frame */
} oe_sbus_frame_t;

typedef struct oe_sbus_config {
    uint32_t baud_rate; /* 0 -> use default 100000 */
    uint8_t nonblocking; /* 1 recommended for timeout loops */
} oe_sbus_config_t;

oe_result_t oe_sbus_query_caps(oe_sbus_caps_t *out_caps);

oe_result_t oe_sbus_open(oe_sbus_t *s, const char *uart_path, const oe_sbus_config_t *cfg);
oe_result_t oe_sbus_close(oe_sbus_t *s);

/* timeout_ms <0: block forever; timeout_ms == 0: try once; timeout_ms >0: up to N ms */
oe_result_t oe_sbus_recv_frame(oe_sbus_t *s, oe_sbus_frame_t *out_frame, int timeout_ms);

#ifdef __cplusplus
}
#endif

#endif /* OPENEMBER_HAL_SBUS_H_ */

