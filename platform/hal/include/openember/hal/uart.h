/* OpenEmber HAL — 总线：UART（C ABI，Linux termios） */
#ifndef OPENEMBER_HAL_UART_H_
#define OPENEMBER_HAL_UART_H_

#include "openember/osal/types.h"

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct oe_uart {
    uint8_t opaque[128];
} oe_uart_t;

typedef enum oe_uart_parity {
    OE_UART_PARITY_NONE = 0,
    OE_UART_PARITY_EVEN = 1,
    OE_UART_PARITY_ODD = 2,
} oe_uart_parity_t;

/* Capability masks for parity support */
typedef enum oe_uart_parity_mask {
    OE_UART_PARITY_MASK_NONE = 1u << 0,
    OE_UART_PARITY_MASK_EVEN = 1u << 1,
    OE_UART_PARITY_MASK_ODD = 1u << 2,
} oe_uart_parity_mask_t;

#ifndef OE_UART_CAPS_MAX_BAUD_RATES
#define OE_UART_CAPS_MAX_BAUD_RATES 8
#endif

typedef struct oe_uart_caps {
    uint32_t baud_rate_count;
    uint32_t baud_rates[OE_UART_CAPS_MAX_BAUD_RATES];
    uint32_t parity_mask; /* oe_uart_parity_mask_t bitmask */
    uint32_t data_bits_min; /* inclusive */
    uint32_t data_bits_max; /* inclusive */
    uint32_t stop_bits_min; /* inclusive */
    uint32_t stop_bits_max; /* inclusive */
} oe_uart_caps_t;

typedef struct oe_uart_config {
    uint32_t baud_rate;
    uint8_t data_bits; /* 通常为 8 */
    uint8_t stop_bits; /* 1 或 2 */
    oe_uart_parity_t parity;
} oe_uart_config_t;

/** path 如 /dev/ttyUSB0、/dev/ttyS0 */
oe_result_t oe_uart_open(oe_uart_t *u, const char *path, const oe_uart_config_t *cfg);
oe_result_t oe_uart_close(oe_uart_t *u);

/** 阻塞读写；*out_* 可为 NULL */
oe_result_t oe_uart_read(oe_uart_t *u, void *buf, size_t len, size_t *out_read);
oe_result_t oe_uart_write(oe_uart_t *u, const void *buf, size_t len, size_t *out_written);

oe_result_t oe_uart_query_caps(oe_uart_caps_t *out_caps);

#ifdef __cplusplus
}
#endif

#endif /* OPENEMBER_HAL_UART_H_ */
