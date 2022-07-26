/*
 * Copyright (c) 2006-2020, DreamGrow Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2022-07-07     luhuadong    the first version
 */

#include <stdio.h>
#include "log_wrapper.h"

int main(void)
{
    printf("[Module] Template\n");

    log_init("Template");

    LOG_I("This is Template");

    log_deinit();

    return 0;
}