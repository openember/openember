/* OpenEmber HAL — Bus: SPI (C ABI, Linux spidev) */
#ifndef OPENEMBER_HAL_SPI_H_
#define OPENEMBER_HAL_SPI_H_

#include "openember/osal/types.h"

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct oe_spi {
    uint8_t opaque[128];
} oe_spi_t;

typedef struct oe_spi_config {
    uint32_t speed_hz;
    uint8_t mode;          /* 0..3 (SPI mode bits) */
    uint8_t bits_per_word;/* 8 typically */
    uint16_t delay_usecs; /* inter-transfer delay */
} oe_spi_config_t;

typedef struct oe_spi_caps {
    uint32_t supports_full_duplex; /* 1 */
    uint32_t supports_mode_count;   /* 0=unknown */
    uint32_t min_bits_per_word;     /* 0=unknown */
    uint32_t max_bits_per_word;     /* 0=unknown */
} oe_spi_caps_t;

oe_result_t oe_spi_query_caps(oe_spi_caps_t *out_caps);

oe_result_t oe_spi_open(oe_spi_t *s, const char *dev_path, const oe_spi_config_t *cfg);
oe_result_t oe_spi_close(oe_spi_t *s);

/**
 * If tx is NULL, send zero bytes.
 * If rx is NULL, receive data is discarded.
 */
oe_result_t oe_spi_transfer(oe_spi_t *s,
                             const void *tx,
                             void *rx,
                             size_t len,
                             size_t *out_transferred);

#ifdef __cplusplus
}
#endif

#endif /* OPENEMBER_HAL_SPI_H_ */

