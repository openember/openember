/* OpenEmber OSAL — 有名管道 FIFO（C ABI, Linux: mkfifo/open/read/write + poll） */
#ifndef OPENEMBER_OSAL_FIFO_H_
#define OPENEMBER_OSAL_FIFO_H_

#include "openember/osal/types.h"

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct oe_fifo {
    uint8_t opaque[512];
} oe_fifo_t;

typedef struct oe_fifo_caps {
    uint32_t supports_fifo; /* 1 */
} oe_fifo_caps_t;

typedef enum oe_fifo_mode {
    OE_FIFO_MODE_READ = 1u << 0,
    OE_FIFO_MODE_WRITE = 1u << 1,
    OE_FIFO_MODE_RDWR = OE_FIFO_MODE_READ | OE_FIFO_MODE_WRITE,
} oe_fifo_mode_t;

oe_result_t oe_fifo_query_caps(oe_fifo_caps_t *out_caps);

/* Create FIFO if missing and open it. mode is a bitmask of oe_fifo_mode_t. */
oe_result_t oe_fifo_open(oe_fifo_t *f, const char *path, uint32_t mode);
oe_result_t oe_fifo_close(oe_fifo_t *f);
oe_result_t oe_fifo_unlink(const char *path);

oe_result_t oe_fifo_write(oe_fifo_t *f,
                            const void *buf,
                            size_t len,
                            size_t *out_written,
                            int timeout_ms);
oe_result_t oe_fifo_read(oe_fifo_t *f,
                           void *buf,
                           size_t len,
                           size_t *out_read,
                           int timeout_ms);

#ifdef __cplusplus
}
#endif

#endif /* OPENEMBER_OSAL_FIFO_H_ */

