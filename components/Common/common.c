/*
 * Copyright (c) 2022-2023, OpenEmber Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2022-08-01     luhuadong    the first version
 */

#include <stdio.h>
#include <assert.h>

void sayHello(const char *name)
{
    assert(name);

    printf("Hello, %s!\n", name);
}