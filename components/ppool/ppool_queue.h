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

#ifndef _PPOOL_QUEUE_H
#define _PPOOL_QUEUE_H

#include <stdlib.h>
#include <string.h>
#include <errno.h>

/* 任务节点 */
typedef struct ppool_node {
    
    void (*entry)(void *parameter); /* 任务 */
    void *parameter;                /* 参数 */
    int   priority;                 /* 优先级 */

    struct ppool_node *next;
} pool_node;

/* 任务列表头指针 */
typedef struct {
    int        len;  /* 任务数量 */
    pool_node *head; /* 列表头指针 */
} pool_queue;

pool_queue *ppool_queue_init(void);
pool_node  *ppool_queue_new(void (*entry)(void *parameter), void *parameter, int priority);
void ppool_queue_add(pool_queue *queue, pool_node *node);
pool_node *ppool_queue_get_task(pool_queue *queue);
void ppool_queue_cleanup(pool_queue *queue);
void ppool_queue_destroy(pool_queue *queue);

#endif
