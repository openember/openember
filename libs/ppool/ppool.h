/*
 * Copyright (c) 2022-2023, OpenEmber Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2015-06-30     briskgreen   the first version
 * 2022-11-02     luhuadong    optimize code style
 */

#ifndef _PPOOL_H
#define _PPOOL_H

#include <pthread.h>

#include "openember.h"
#include "ppool_queue.h"

#ifdef __cplusplus
extern "C" {
#endif

pthread_mutex_t PPOOL_LOCK;

#define ppool_entry() pthread_mutex_lock(&PPOOL_LOCK)
#define ppool_leave() pthread_mutex_unlock(&PPOOL_LOCK)

typedef struct {
    int pool_max_num;  /* 线程池最大线程数量 */
    int rel_num;       /* 线程池中实例线程数 */
    pool_queue *queue; /* 任务队列头指针 */
    pthread_t  *id;    /* 线程 ID 号 */

    pthread_mutex_t ppool_lock;
    pthread_cond_t  ppool_cond;
} pool_t;

typedef struct {
    void (*entry)(void *parameter); /* 任务 */
    void  *parameter;               /* 参数 */
    int    priority;                /* 优先级 */
} pool_task;

pool_t   *ppool_init(int pool_max_num);
ag_bool_t ppool_add(pool_t *pool, pool_task *task);
void      ppool_destroy(pool_t *pool);

#ifdef __cplusplus
}
#endif

#endif
