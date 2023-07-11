/*
 * Copyright (c) 2022-2023, OpenEmber Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2022-07-26     luhuadong    the first version
 */

#include "agloo.h"

#ifdef AG_LIBS_USING_ZLOG
#include "zlog.h"
#endif

#ifdef AG_LIBS_USING_EASYLOGGER
#include "elog.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#endif

int log_init(const char *name)
{
    int rc;

#if defined(AG_LIBS_USING_ZLOG)

    rc = dzlog_init(LOG_FILE, name);
    if (rc) {
        printf("init failed, please check file %s.\n", LOG_FILE);
        return -AG_ERROR;
    }

#elif defined(AG_LIBS_USING_EASYLOGGER)
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
    return AG_EOK;
}

void log_deinit(void)
{
#if defined(AG_LIBS_USING_ZLOG)
    zlog_fini();
#elif defined(AG_LIBS_USING_EASYLOGGER)
    elog_stop();
#endif
}