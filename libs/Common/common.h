/*
 * Copyright (c) 2006-2020, DreamGrow Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2022-08-01     luhuadong    the first version
 */

#ifndef __AG_COMMON_H__
#define __AG_COMMON_H__

#include "agloo.h"

#ifdef __cplusplus
extern "C" {
#endif

void sayHello(const char *name);

int ag_random(void);
int ag_time(void);

#ifdef __cplusplus
}
#endif

#endif /* __AG_COMMON_H__ */