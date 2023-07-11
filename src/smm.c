/*
 * Copyright (c) 2022-2023, OpenEmber Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2022-08-01     luhuadong    the first version
 */

#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <signal.h>
#include <assert.h>

#define LOG_TAG                "Workflow"
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

//int smm_register(smm_t *module, const char *name, const int pid, void *user_data)
smm_t *smm_register(const char *name, const mod_class_t cls, const int pid, void *user_data)
{
    assert(name);

    smm_t *module = calloc(1, sizeof(smm_t));
    if (module == NULL) {
        return NULL;
    }

    strncpy(module->name, name, sizeof(module->name)-1);
    module->cls = cls;
    module->pid = pid;
    module->priority = SUBMODULE_PRIO_1;
    module->user_data = NULL;
    INIT_LIST_HEAD(&(module->list));

    /* lock */
    pthread_rwlock_wrlock(&rwlock_mod);

    /* Add current device to device list */
    list_add_tail(&(module->list), &submodule_list);
    g_device_num += 1;

    /* unlock */
    pthread_rwlock_unlock(&rwlock_mod);

    module->status = AG_MODULE_INIT;

    return module;
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
    LOG_D("Submodule init");
    return AG_EOK;
}

int smm_kill_all_modules(void)
{
    // 遍历 submodule_list
    smm_t *module = NULL;
    LOG_D("Kill all modules");

    pthread_rwlock_rdlock(&rwlock_mod);

    list_for_each_entry(module, &submodule_list, list)
    {
        LOG_D("!_ %s: %d", module->name, module->pid);
        kill(module->pid, SIGKILL);
    }

    pthread_rwlock_unlock(&rwlock_mod);
    return AG_EOK;
}

int smm_stop_all_modules(void)
{
    return AG_EOK;
}

#ifdef __cplusplus
}
#endif
