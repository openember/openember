/*
 * Copyright (c) 2017-2022, DreamGrow Team
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
#include "ppool_queue.h"
#include "agloo.h"

pthread_mutex_t PPOOL_LOCK;

#define ppool_entry() pthread_mutex_lock(&PPOOL_LOCK)
#define ppool_leave() pthread_mutex_unlock(&PPOOL_LOCK)

typedef struct {
    int pool_max_num; //线程池最大线程数量
    int rel_num;      //线程池中实例线程数
    pool_w *head;     //线程头
    pthread_t *id;    //线程id

    pthread_mutex_t ppool_lock;
    pthread_cond_t  ppool_cond;
} pool_t;

//任务数据结构
typedef struct {
    int priority;    //优先级
    ppool_work task; //任务
    void *arg;       //参数
}pool_task;

//初始化一个线程池
pool_t *ppool_init(int pool_max_num);

//向线程池中添加一个任务
ag_bool_t ppool_add(pool_t *pool,pool_task *task);

//销毁一个线程池
void ppool_destroy(pool_t *pool);

#endif
