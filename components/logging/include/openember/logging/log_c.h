/*
 * OpenEmber logging C ABI — for C translation units only.
 * C++ code should include openember/logging/log.hpp.
 */
#ifndef OPENEMBER_LOGGING_LOG_C_H_
#define OPENEMBER_LOGGING_LOG_C_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdarg.h>

typedef enum oe_log_level {
    OE_LOG_LEVEL_DEBUG = 10,
    OE_LOG_LEVEL_INFO  = 20,
    OE_LOG_LEVEL_WARN  = 30,
    OE_LOG_LEVEL_ERROR = 40,
} oe_log_level_t;

int oe_log_init(const char *process_name);
void oe_log_deinit(void);
int log_init(const char *name);
void log_deinit(void);

void oe_log_vwrite(oe_log_level_t level, const char *tag, const char *fmt, va_list ap);
void oe_log_write(oe_log_level_t level, const char *tag, const char *fmt, ...);

#ifdef __cplusplus
}
#endif

#ifndef LOG_TAG
#define LOG_TAG ""
#endif

#ifndef OE_LOG_TAG
#define OE_LOG_TAG LOG_TAG
#endif

#define OE_LOGD(...) oe_log_write(OE_LOG_LEVEL_DEBUG, OE_LOG_TAG, __VA_ARGS__)
#define OE_LOGI(...) oe_log_write(OE_LOG_LEVEL_INFO, OE_LOG_TAG, __VA_ARGS__)
#define OE_LOGW(...) oe_log_write(OE_LOG_LEVEL_WARN, OE_LOG_TAG, __VA_ARGS__)
#define OE_LOGE(...) oe_log_write(OE_LOG_LEVEL_ERROR, OE_LOG_TAG, __VA_ARGS__)

#ifndef LOG_D
#define LOG_D(...) OE_LOGD(__VA_ARGS__)
#endif
#ifndef LOG_I
#define LOG_I(...) OE_LOGI(__VA_ARGS__)
#endif
#ifndef LOG_W
#define LOG_W(...) OE_LOGW(__VA_ARGS__)
#endif
#ifndef LOG_E
#define LOG_E(...) OE_LOGE(__VA_ARGS__)
#endif

#endif /* OPENEMBER_LOGGING_LOG_C_H_ */
