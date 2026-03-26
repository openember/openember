/*
 * OpenEmber logging C ABI implementation (process-local).
 *
 * This file lives in components/Log and is linked into each process that uses
 * OpenEmber runtime. Platform code should only include openember/log_c.h.
 */
#include "openember/log_c.h"

#include "log_wrapper.h" /* backend selection + legacy init */

#include <stdio.h>
#include <string.h>

#if defined(EMBER_LIBS_USING_SPDLOG)
/* Tag-aware functions implemented by log_spdlog.cpp */
void ember_log_debug_tag(const char *tag, const char *fmt, ...);
void ember_log_info_tag(const char *tag, const char *fmt, ...);
void ember_log_warn_tag(const char *tag, const char *fmt, ...);
void ember_log_error_tag(const char *tag, const char *fmt, ...);
#endif

int oe_log_init(const char *process_name)
{
    return log_init(process_name);
}

void oe_log_deinit(void)
{
    log_deinit();
}

static const char *oe_log_safe_tag(const char *tag)
{
    return (tag && tag[0]) ? tag : "";
}

void oe_log_vwrite(oe_log_level_t level, const char *tag, const char *fmt, va_list ap)
{
    const char *t = oe_log_safe_tag(tag);

#if defined(EMBER_LIBS_USING_SPDLOG)
    /* We must re-expand va_list for the final call, so format once here. */
    char msg[2048];
    vsnprintf(msg, sizeof(msg), fmt ? fmt : "", ap);

    switch (level) {
    case OE_LOG_LEVEL_DEBUG:
        ember_log_debug_tag(t, "%s", msg);
        break;
    case OE_LOG_LEVEL_INFO:
        ember_log_info_tag(t, "%s", msg);
        break;
    case OE_LOG_LEVEL_WARN:
        ember_log_warn_tag(t, "%s", msg);
        break;
    case OE_LOG_LEVEL_ERROR:
    default:
        ember_log_error_tag(t, "%s", msg);
        break;
    }
#else
    /* Fallback: best-effort stderr print with tag prefix. */
    (void)level;
    fprintf(stderr, "[%s] ", t);
    vfprintf(stderr, fmt ? fmt : "", ap);
    fprintf(stderr, "\n");
#endif
}

void oe_log_write(oe_log_level_t level, const char *tag, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    oe_log_vwrite(level, tag, fmt, ap);
    va_end(ap);
}

