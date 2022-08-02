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

AG_EXT char *class_label[SUBMODULE_CLASS_MAX];

/*
 * 组织形式：链表 or 数组？
 * 排列依据：
 * 1. 按优先级排列
 * 2. 按模块类型排列
 * 3. 按注册顺序排列
 * 
 * 注意：同一模块可能需要运行多个实例
 * 
 * 关键：模块管理的最小单元？
 */

typedef struct submodule_manager
{
    char name[AG_NAME_MAX];   /* Submodule name */
    mod_class_t class;        /* Submodule class */
    pid_t pid;                /* Submodule process id */
    mod_prio_t priority;      /* Submodule priority */
    mod_state_t status;       /* Submodule status */
    struct list_head list;    /* Device list */
    void *user_data;          /* User-specific data */
} smm_t;

/* Register and unregister submodules */
smm_t *smm_register(const char *name, const mod_class_t class, const int pid, void *user_data);
int smm_unregister(smm_t *module);

int smm_init(void);
int smm_stop_all_modules(void);
int smm_kill_all_modules(void);

#ifdef __cplusplus
}
#endif

#endif /* __SUBMODULE_MANAGER_H__ */