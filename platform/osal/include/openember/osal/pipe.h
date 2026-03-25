/* OpenEmber OSAL — 管道（匿名 pipe）（C ABI, Linux: pipe + poll） */
#ifndef OPENEMBER_OSAL_PIPE_H_
#define OPENEMBER_OSAL_PIPE_H_

#include "openember/osal/types.h"

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct oe_pipe {
    uint8_t opaque[256];
} oe_pipe_t;

typedef struct oe_pipe_caps {
    uint32_t supports_pipe; /* 1 */
} oe_pipe_caps_t;

oe_result_t oe_pipe_query_caps(oe_pipe_caps_t *out_caps);

oe_result_t oe_pipe_create(oe_pipe_t *p);
oe_result_t oe_pipe_close(oe_pipe_t *p);

oe_result_t oe_pipe_write(oe_pipe_t *p,
                            const void *buf,
                            size_t len,
                            size_t *out_written,
                            int timeout_ms);

oe_result_t oe_pipe_read(oe_pipe_t *p,
                           void *buf,
                           size_t len,
                           size_t *out_read,
                           int timeout_ms);

#ifdef __cplusplus
}
#endif

#endif /* OPENEMBER_OSAL_PIPE_H_ */

