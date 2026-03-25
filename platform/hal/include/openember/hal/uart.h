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

#ifdef __cplusplus
}
#endif

#endif /* OPENEMBER_HAL_UART_H_ */
