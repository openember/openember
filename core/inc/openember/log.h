/*
 * OpenEmber logging facade (C/C++).
 *
 * Prefer these macros in apps/modules/components. Platform code should use
 * the C ABI in openember/log_c.h (to avoid depending on any concrete backend).
 */
#ifndef OPENEMBER_LOG_H_
#define OPENEMBER_LOG_H_

#include "openember/log_c.h"

/* Default tag if a TU didn't define one. */
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

/* Back-compat aliases (existing code uses LOG_*). */
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

#endif /* OPENEMBER_LOG_H_ */

