/* OpenEmber OSAL — 信号（Linux: signalfd，薄封装 + 同步等待） */
#ifndef OPENEMBER_OSAL_SIGNALS_H_
#define OPENEMBER_OSAL_SIGNALS_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "openember/osal/types.h"

#include <stdint.h>
#include <stddef.h>
#include <sys/types.h>

typedef struct oe_signals {
    uint8_t opaque[256];
} oe_signals_t;

typedef struct oe_signals_caps {
    uint32_t supports_signalfd; /* 1 */
} oe_signals_caps_t;

typedef struct oe_signal_info {
    int signum;
    pid_t sender_pid;
} oe_signal_info_t;

oe_result_t oe_signals_query_caps(oe_signals_caps_t *out_caps);

/* signums: array of signal numbers (e.g. {SIGUSR1}). */
oe_result_t oe_signals_open(oe_signals_t *s, const int *signums, size_t count);
oe_result_t oe_signals_wait(oe_signals_t *s, oe_signal_info_t *out_info, int timeout_ms);
oe_result_t oe_signals_close(oe_signals_t *s);

#ifdef __cplusplus
}
#endif

#endif /* OPENEMBER_OSAL_SIGNALS_H_ */

