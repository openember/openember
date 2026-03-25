/* OpenEmber OSAL — POSIX Message Queue (C ABI, Linux: mq_*) */
#ifndef OPENEMBER_OSAL_MQ_H_
#define OPENEMBER_OSAL_MQ_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "openember/osal/types.h"

#include <stdint.h>
#include <stddef.h>

typedef struct oe_mq {
    uint8_t opaque[256];
} oe_mq_t;

typedef struct oe_mq_caps {
    uint32_t supports_message_queue; /* 1 */
    uint32_t max_msg_size;           /* 0=unknown */
    uint32_t max_msgs;               /* 0=unknown */
} oe_mq_caps_t;

oe_result_t oe_mq_query_caps(oe_mq_caps_t *out_caps);

oe_result_t oe_mq_create(oe_mq_t *mq,
                           const char *name,
                           uint32_t max_msgs,
                           uint32_t msg_size);

oe_result_t oe_mq_open(oe_mq_t *mq, const char *name);
oe_result_t oe_mq_close(oe_mq_t *mq);
oe_result_t oe_mq_unlink(const char *name);

oe_result_t oe_mq_send(oe_mq_t *mq,
                         const void *buf,
                         size_t len,
                         int timeout_ms);

oe_result_t oe_mq_recv(oe_mq_t *mq,
                         void *buf,
                         size_t cap,
                         size_t *out_len,
                         int timeout_ms);

#ifdef __cplusplus
}
#endif

#endif /* OPENEMBER_OSAL_MQ_H_ */

