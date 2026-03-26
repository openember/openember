/*
 * Copyright (c) 2022-2023, OpenEmber Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2022-07-26     luhuadong    the first version
 */

#include "openember.h"

#include <stdio.h>

#ifdef EMBER_LIBS_USING_ZLOG
#include "zlog.h"
#endif

#if defined(EMBER_LIBS_USING_LOG_BUILTIN)
#include <stdarg.h>
#include <stdio.h>

static void ember_builtin_vlog(const char *prefix, FILE *out, const char *fmt, va_list ap)
{
    fprintf(out, "%s", prefix);
    vfprintf(out, fmt, ap);
    fprintf(out, "\n");
}

void ember_builtin_log_debug(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    ember_builtin_vlog("[D] ", stderr, fmt, ap);
    va_end(ap);
}

void ember_builtin_log_info(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    ember_builtin_vlog("[I] ", stderr, fmt, ap);
    va_end(ap);
}

void ember_builtin_log_warn(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    ember_builtin_vlog("[W] ", stderr, fmt, ap);
    va_end(ap);
}

void ember_builtin_log_error(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    ember_builtin_vlog("[E] ", stderr, fmt, ap);
    va_end(ap);
}
#endif

#if defined(EMBER_LIBS_USING_SPDLOG)
extern int log_spdlog_init(const char *name);
extern void log_spdlog_deinit(void);
#endif

int log_init(const char *name)
{
#if defined(EMBER_LIBS_USING_ZLOG)

    int rc = dzlog_init(LOG_FILE, name);
    if (rc) {
        printf("init failed, please check file %s.\n", LOG_FILE);
        return -EMBER_ERROR;
    }

#elif defined(EMBER_LIBS_USING_SPDLOG)

    if (log_spdlog_init(name)) {
        printf("init failed (spdlog).\n");
        return -EMBER_ERROR;
    }

#elif defined(EMBER_LIBS_USING_LOG_BUILTIN)

    (void)name;

#endif
    return EMBER_EOK;
}

void log_deinit(void)
{
#if defined(EMBER_LIBS_USING_ZLOG)
    zlog_fini();
#elif defined(EMBER_LIBS_USING_SPDLOG)
    log_spdlog_deinit();
#endif
}
