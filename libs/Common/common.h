/*
 * Copyright (c) 2022-2023, OpenEmber Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2022-08-01     luhuadong    the first version
 */

#ifndef __EMBER_COMMON_H__
#define __EMBER_COMMON_H__

#include "openember.h"

#ifdef __cplusplus
extern "C" {
#endif

void sayHello(const char *name);

int ember_random(void);
int ember_time(void);

#ifdef __cplusplus
}
#endif

#endif /* __EMBER_COMMON_H__ */