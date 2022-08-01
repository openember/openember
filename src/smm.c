/*
 * Copyright (c) 2006-2020, DreamGrow Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2022-08-01     luhuadong    the first version
 */

#include <pthread.h>

#include "agloo.h"

#define AG_GLOBALS
#include "smm.h"

#ifdef __cplusplus
extern "C" {
#endif

/* The global list and lock */
//static struct list_head submodule_list = LIST_HEAD_INIT(submodule_list);
//static pthread_rwlock_t rwlock_mod = PTHREAD_RWLOCK_INITIALIZER;

int smm_init(void)
{
    printf("Submodule init\n");
    return AG_EOK;
}

#ifdef __cplusplus
}
#endif
