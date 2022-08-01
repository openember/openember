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
#include <assert.h>

#include "agloo.h"

#define AG_GLOBALS
#include "smm.h"

#ifdef __cplusplus
extern "C" {
#endif

/* The global list and lock */
static struct list_head submodule_list = LIST_HEAD_INIT(submodule_list);
static pthread_rwlock_t rwlock_mod = PTHREAD_RWLOCK_INITIALIZER;

static uint16_t g_device_num = 0;

struct list_head *get_submodule_list(void)
{
    return &submodule_list;
}

int smm_register(smm_t *module, const char *name, const int pid, void *user_data)
{
    assert(module);
    assert(name);

    module->name = name;
    module->pid = pid;

    INIT_LIST_HEAD(&module->list);

    /* lock */
    pthread_rwlock_wrlock(&rwlock_mod);

    /* Add current device to device list */
    list_add_tail(&(module->list), &submodule_list);
    g_device_num += 1;

    /* unlock */
    pthread_rwlock_unlock(&rwlock_mod);

    module->status = AG_MODULE_INIT;

    return AG_EOK;
}

int smm_unregister(smm_t *module)
{
    if (module == NULL)
        return -AG_EINVAL;
    
    pthread_rwlock_wrlock(&rwlock_mod);
    list_del(&(module->list));
    g_device_num -= 1;
    pthread_rwlock_unlock(&rwlock_mod);

    return AG_EOK;
}

int smm_init(void)
{
    printf("Submodule init\n");
    return AG_EOK;
}

#ifdef __cplusplus
}
#endif
