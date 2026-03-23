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

#include "ppool_queue.h"
#include "ppool_errno.h"

/**
 * This function query the insert position of pool queue 
 * 查询插入的位置
 *
 * @param head     the header pointer of pool
 * @param priority the priority which you want to query
 * 
 * @return The position number where to insert the node if success,
 *         return -1 if failure.
 */
static int ppool_queue_get_insert_pos(pool_node *head, int priority)
{
    int pos;

    if (head == NULL) {
        return -1;
    }

    for (pos=0; head && priority >= head->priority; ++pos) {
        head = head->next;
    }

    return pos;
}

/**
 * This function will init pthread pool queue
 * 初始化一个任务列表
 *
 * @return The handle of pool queue if success,
 *         return NULL if failure.
 */
pool_queue *ppool_queue_init(void)
{
    pool_queue *head;

    head = malloc(sizeof(pool_queue));
    if (!head) {
        ppool_errno = PE_QUEUE_NO_MEM;
        return NULL;
    }

    head->len = 0;
    head->head = NULL;

    return head;
}

/**
 * This function will create a new pool node
 * 查询插入的位置
 *
 * @param task      the task function entry
 * @param parameter the parameter pass to task function
 * @param priority  the priority of this task
 * 
 * @return The handle of a new node if success,
 *         return NULL if failure.
 */
pool_node *ppool_queue_new(void (*entry)(void *parameter), void *parameter, int priority)
{
    pool_node *node;

    if (priority < 0) {
        ppool_errno = PE_PRIORITY_ERROR;
        return NULL;
    }

    node = malloc(sizeof(pool_node));
    if (node == NULL) {
        ppool_errno = PE_QUEUE_NODE_NO_MEM;
        return NULL;
    }

    node->entry = entry;
    node->parameter = parameter;
    node->priority  = priority;
    node->next = NULL;

    return node;
}

/**
 * This function will add a node into the pool queue
 * 添加一个任务到任务队列
 *
 * @param queue the header pointer of pool
 * @param node  the node that will insert
 * 
 * @return None
 */
void ppool_queue_add(pool_queue *queue, pool_node *node)
{
    int pos;
    int i;
    pool_node *h = queue->head;

    pos = ppool_queue_get_insert_pos(h, node->priority);

    if (pos == -1) {
        queue->head = node;
        queue->len += 1;
        return;
    }
    else if (pos == 0) {
        node->next  = h;
        queue->head = node;
        queue->len += 1;
        return;
    }

    for(i=0; i < pos-1; ++i) {
        h = h->next;
    }

    node->next  = h->next;
    h->next     = node;
    queue->len += 1;
}

/**
 * This function get a task which has highest priority from the pool queue 
 * 从任务队列中获取一个任务
 *
 * @param queue the header pointer of pool
 * 
 * @return The handle of task node if success,
 *         return NULL if failure.
 */
pool_node *ppool_queue_get_task(pool_queue *queue)
{
    pool_node *task;

    if (queue->head == NULL) {
        return NULL;
    }

    task = queue->head;
    queue->head = queue->head->next;
    queue->len -= 1;

    return task;
}

/**
 * This function will clean up the pool queue
 * 清空任务队列
 *
 * @param queue the header pointer of pool
 * 
 * @return None
 */
void ppool_queue_cleanup(pool_queue *queue)
{
    pool_node *h = queue->head;
    pool_node *temp;

    while (h) {
        temp = h;
        h = h->next;

        free(temp);
    }

    queue->len  = 0;
    queue->head = NULL;
}

/**
 * This function will destroy the pool queue 
 * 销毁任务队列
 *
 * @param queue the header pointer of pool
 * 
 * @return None
 */
void ppool_queue_destroy(pool_queue *queue)
{
    pool_node *h = queue->head;
    pool_node *temp;

    while (h) {
        temp = h;
        h = h->next;

        free(temp);
    }

    free(queue);
}
