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

#ifdef EMBER_LIBS_USING_EASYLOGGER
#include "elog.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
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
    int rc;

#if defined(EMBER_LIBS_USING_ZLOG)

    rc = dzlog_init(LOG_FILE, name);
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

#elif defined(EMBER_LIBS_USING_EASYLOGGER)
    /* close printf buffer */
    setbuf(stdout, NULL);
    /* initialize EasyLogger */
    rc = elog_init();
    /* set EasyLogger log format */
    elog_set_fmt(ELOG_LVL_ASSERT, ELOG_FMT_ALL);
    elog_set_fmt(ELOG_LVL_ERROR, ELOG_FMT_LVL | ELOG_FMT_TAG | ELOG_FMT_TIME);
    elog_set_fmt(ELOG_LVL_WARN, ELOG_FMT_LVL | ELOG_FMT_TAG | ELOG_FMT_TIME);
    elog_set_fmt(ELOG_LVL_INFO, ELOG_FMT_LVL | ELOG_FMT_TAG | ELOG_FMT_TIME);
    elog_set_fmt(ELOG_LVL_DEBUG, ELOG_FMT_ALL & ~ELOG_FMT_FUNC);
    elog_set_fmt(ELOG_LVL_VERBOSE, ELOG_FMT_ALL & ~ELOG_FMT_FUNC);
#ifdef ELOG_COLOR_ENABLE
    elog_set_text_color_enabled(true);
#endif
    /* start EasyLogger */
    elog_start();

#endif
    return EMBER_EOK;
}

void log_deinit(void)
{
#if defined(EMBER_LIBS_USING_ZLOG)
    zlog_fini();
#elif defined(EMBER_LIBS_USING_SPDLOG)
    log_spdlog_deinit();
#elif defined(EMBER_LIBS_USING_EASYLOGGER)
    elog_stop();
#endif
}
