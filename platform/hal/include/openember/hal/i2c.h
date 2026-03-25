/* OpenEmber HAL — Bus: I2C (C ABI, Linux i2c-dev) */
#ifndef OPENEMBER_HAL_I2C_H_
#define OPENEMBER_HAL_I2C_H_

#include "openember/osal/types.h"

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct oe_i2c {
    uint8_t opaque[128];
} oe_i2c_t;

typedef struct oe_i2c_caps {
    uint32_t max_write_bytes; /* 0 = unknown/unlimited */
    uint32_t max_read_bytes;  /* 0 = unknown/unlimited */
    uint32_t supports_write_read; /* 1 = supported */
} oe_i2c_caps_t;

oe_result_t oe_i2c_query_caps(oe_i2c_caps_t *out_caps);

oe_result_t oe_i2c_open(oe_i2c_t *i2c, uint32_t bus_num, uint8_t addr_7bit);
oe_result_t oe_i2c_close(oe_i2c_t *i2c);

oe_result_t oe_i2c_write(oe_i2c_t *i2c, const uint8_t *data, size_t len, size_t *out_written);
oe_result_t oe_i2c_read(oe_i2c_t *i2c, uint8_t *data, size_t len, size_t *out_read);

/* Common I2C pattern: write (register/address) then repeated-start read. */
oe_result_t oe_i2c_write_read(oe_i2c_t *i2c,
                                const uint8_t *wbuf,
                                size_t wlen,
                                uint8_t *rbuf,
                                size_t rlen);

#ifdef __cplusplus
}
#endif

#endif /* OPENEMBER_HAL_I2C_H_ */

