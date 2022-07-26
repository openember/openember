/*
 * Copyright (c) 2006-2020, DreamGrow Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2022-07-26     luhuadong    the first version
 */

#include "agloo.h"
#include "zlog.h"

int log_init(const char *name)
{
#if 1
    int rc;

    rc = dzlog_init(LOG_FILE, name);
    if (rc) {
        printf("init failed, please check file %s.\n", LOG_FILE);
        return -AG_ERROR;
    }
#else

#endif
    return AG_EOK;
}

void log_deinit(void)
{
    zlog_fini();
}