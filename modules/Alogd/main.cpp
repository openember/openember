/*
 * Copyright (c) 2022-2023, OpenEmber Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2022-08-08     luhuadong    the first version
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define MODULE_NAME            "alogd"
#define LOG_TAG                MODULE_NAME
#include "agloo.h"


int main(void)
{
    int rc;
    
    log_init(MODULE_NAME);
    LOG_I("Version: %lu.%lu.%lu", AG_VERSION, AG_SUBVERSION, AG_REVISION);

    daemon(0, 0);

    while (1) {
        sleep(1);
    }

    return 0;
}