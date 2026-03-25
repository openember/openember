/* OpenEmber HAL — Bus: CAN / CAN-FD (C ABI, SocketCAN) */
#ifndef OPENEMBER_HAL_CAN_H_
#define OPENEMBER_HAL_CAN_H_

#include "openember/osal/types.h"

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct oe_can {
    uint8_t opaque[128];
} oe_can_t;

typedef struct oe_can_config {
    uint32_t enable_can_fd; /* 0/1 */
} oe_can_config_t;

typedef struct oe_can_frame {
    uint32_t can_id; /* raw ID without CAN_EFF_FLAG */
    uint8_t data_len; /* bytes: 0..8 for classic, 0..64 for fd */
    uint8_t is_extended; /* 0=std, 1=extended */
    uint8_t is_fd; /* 0=classic, 1=fd */
    uint8_t data[64];
} oe_can_frame_t;

typedef struct oe_can_caps {
    uint32_t supports_can_fd; /* 0/1 */
    uint32_t max_data_len_can;    /* classic max len */
    uint32_t max_data_len_can_fd; /* fd max len */
} oe_can_caps_t;

oe_result_t oe_can_query_caps(oe_can_caps_t *out_caps);

oe_result_t oe_can_open(oe_can_t *c, const char *ifname, const oe_can_config_t *cfg);
oe_result_t oe_can_close(oe_can_t *c);

oe_result_t oe_can_send_frame(oe_can_t *c, const oe_can_frame_t *frame);

/* timeout_ms <0: block forever; timeout_ms == 0: poll once; timeout_ms > 0: poll up to N ms */
oe_result_t oe_can_recv_frame(oe_can_t *c, oe_can_frame_t *out_frame, int timeout_ms);

#ifdef __cplusplus
}
#endif

#endif /* OPENEMBER_HAL_CAN_H_ */

