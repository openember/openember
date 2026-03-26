/*
 * OpenEmber logging C ABI (process-local).
 *
 * This header provides a stable, backend-agnostic C interface for logging.
 * The actual backend is selected by CMake (OPENEMBER_LOG_BACKEND).
 */
#ifndef OPENEMBER_LOG_C_H_
#define OPENEMBER_LOG_C_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdarg.h>

typedef enum oe_log_level {
    OE_LOG_LEVEL_DEBUG = 10,
    OE_LOG_LEVEL_INFO = 20,
    OE_LOG_LEVEL_WARN = 30,
    OE_LOG_LEVEL_ERROR = 40,
} oe_log_level_t;

/* Initialize logging for current process. Safe to call multiple times. */
int oe_log_init(const char *process_name);
void oe_log_deinit(void);

/* Tag-aware logging. Tag can be NULL/empty. */
void oe_log_vwrite(oe_log_level_t level, const char *tag, const char *fmt, va_list ap);
void oe_log_write(oe_log_level_t level, const char *tag, const char *fmt, ...);

#ifdef __cplusplus
}
#endif

#endif /* OPENEMBER_LOG_C_H_ */

