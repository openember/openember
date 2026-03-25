/* OpenEmber HAL — 1-Wire (C ABI, Linux w1 sysfs) */
#ifndef OPENEMBER_HAL_ONEWIRE_H_
#define OPENEMBER_HAL_ONEWIRE_H_

#include "openember/osal/types.h"

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct oe_onewire {
    uint8_t opaque[512];
} oe_onewire_t;

typedef struct oe_onewire_caps {
    uint32_t supports_temperature; /* 1 */
    uint32_t supports_raw_read;     /* 1 */
} oe_onewire_caps_t;

typedef struct oe_onewire_temp {
    uint8_t crc_ok;          /* parsed from w1_slave line1 "YES"/"NO" */
    int32_t temperature_milli_c; /* e.g. t=21562 => 21.562C */
    float temperature_c;    /* convenience: temperature_milli_c / 1000 */
} oe_onewire_temp_t;

/* w1_slave sysfs file path, e.g. /sys/bus/w1/devices/28-xxxx/w1_slave */
oe_result_t oe_onewire_open(oe_onewire_t *w, const char *w1_slave_path);
oe_result_t oe_onewire_close(oe_onewire_t *w);

oe_result_t oe_onewire_query_caps(oe_onewire_caps_t *out_caps);

/* Read temperature from w1_slave ("t=" field). Always fills out_temp when possible.
 * Return OE_OK even if CRC is NO (out_temp->crc_ok indicates validity).
 */
oe_result_t oe_onewire_read_temperature(oe_onewire_t *w, oe_onewire_temp_t *out_temp);

/* Read raw two-line payload from w1_slave. out_buf always NUL-terminated on success. */
oe_result_t oe_onewire_read_raw(oe_onewire_t *w, char *out_buf, size_t cap, size_t *out_len);

#ifdef __cplusplus
}
#endif

#endif /* OPENEMBER_HAL_ONEWIRE_H_ */

