/*
 * Copyright (c) 2006-2020, DreamGrow Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2022-08-01     luhuadong    the first version
 */

#ifndef __SUBMODULE_MANAGER_H__
#define __SUBMODULE_MANAGER_H__

#include <sys/types.h>
#include <unistd.h>
#include "agloo.h"

#ifdef __cplusplus
extern "C" {
#endif

AG_EXT char *class_label[8];

typedef struct submodule_manager
{
    const char *name;         /* Submodule name */
    pid_t pid;                /* Submodule process id */
    mod_prio_t priority;      /* Submodule priority */
    mod_state_t status;       /* Submodule status */
    struct list_head list;    /* Device list */
    void *user_data;          /* User-specific data */
} smm_t;

/* Register and unregister submodules */
int smm_register(smm_t *module, const char *name, const int pid, void *user_data);
int smm_unregister(smm_t *module);

int smm_init(void);

#ifdef __cplusplus
}
#endif

#endif /* __SUBMODULE_MANAGER_H__ */