/*
 * Copyright (c) 2022-2023, OpenEmber Team
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

    LOG_D("Hello OpenEmber!");
    LOG_I("Hello OpenEmber!");
    LOG_W("Hello OpenEmber!");
    LOG_E("Hello OpenEmber!");

    log_deinit();

    return AG_EOK;
}