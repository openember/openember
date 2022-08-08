/*
 * Copyright (c) 2006-2020, DreamGrow Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2022-08-08     luhuadong    the first version
 */

#define LOG_TAG  "test"
#include "agloo.h"

int main(void)
{
    log_init(LOG_TAG);

    LOG_D("Hello Agloo!");
    LOG_I("Hello Agloo!");
    LOG_W("Hello Agloo!");
    LOG_E("Hello Agloo!");

    log_deinit();

    return AG_EOK;
}