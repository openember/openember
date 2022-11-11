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

#include <sys/prctl.h>

#include "ppool.h"
#include "ppool_errno.h"

/**
 * 线程池执行任务函数
 *
 * @param pool_max_num the max number of pool
 *
 * @return The handle of pool while success, 
 *         NULL while failure.
 */
static void ppool_run(pool_t *pool)
{
    pool_node *task;

    prctl(PR_SET_NAME, "threadpool");

    while(1)
    {
        while (pthread_mutex_lock(&pool->ppool_lock) != 0);

        while (pool->queue->len <= 0) {
            pthread_cond_wait(&pool->ppool_cond, &pool->ppool_lock);
        }
        task = ppool_queue_get_task(pool->queue);

        while (pthread_mutex_unlock(&pool->ppool_lock) != 0);

        if (task == NULL) continue;
        task->entry(task->parameter);

        free(task);
    }
}

/**
 * This function will init pthread pool
 * 初始化一个线程池
 *
 * @param pool_max_num the max number of pool
 *
 * @return The handle of pool while success, 
 *         NULL while failure.
 */
pool_t *ppool_init(int pool_max_num)
{
    pool_t     *pool;
    pool_queue *queue;
    int i;

    pool = malloc(sizeof(pool_t));
    if (!pool) {
        ppool_errno = PE_POOL_NO_MEM;
        return NULL;
    }

    //创建任务队列
    queue = ppool_queue_init();
    if (!queue) {
        free(pool);
        return NULL;
    }

    pool->pool_max_num = pool_max_num;
    pool->rel_num = 0;
    pool->queue = queue;
    pool->id = malloc(sizeof(pthread_t) * pool_max_num);
    if (!pool->id) {
        ppool_errno = PE_THREAD_NO_MEM;
        free(queue);
        free(pool);
        return NULL;
    }

    if (pthread_mutex_init(&pool->ppool_lock, NULL) != 0) {
        ppool_errno = PE_THREAD_MUTEX_ERROR;
        free(pool->id);
        free(queue);
        free(pool);
        return NULL;
    }

    if (pthread_mutex_init(&PPOOL_LOCK, NULL) != 0) {
        ppool_errno = PE_THREAD_MUTEX_ERROR;
        free(pool->id);
        free(queue);
        pthread_mutex_destroy(&pool->ppool_lock);
        free(pool);
    }

    if (pthread_cond_init(&pool->ppool_cond, NULL) != 0) {
        ppool_errno = PE_THREAD_COND_ERROR;
        free(pool->id);
        free(queue);
        pthread_mutex_destroy(&pool->ppool_lock);
        free(pool);
        return NULL;
    }

    /* Create threads */
    for (i=0; i < pool_max_num; ++i) {

        if (pthread_create(&pool->id[i], NULL, (void *)ppool_run, pool) == 0) {
            ++pool->rel_num;
        }

        pthread_detach(pool->id[i]);
    }

    return pool;
}

/**
 * This function will add a task into pthread pool
 * 向线程池中添加一个任务
 *
 * @param pool the pthread pool handle
 * @param task the task
 *
 * @return AG_TRUE while success, 
 *         AG_FALSE while failure.
 */
ag_bool_t ppool_add(pool_t *pool, pool_task *task)
{
    pool_node *node;

    node = ppool_queue_new(task->entry, task->parameter, task->priority);
    if (!node) {
        return AG_FALSE;
    }

    while (pthread_mutex_lock(&pool->ppool_lock) != 0);

    ppool_queue_add(pool->queue, node);

    while (pthread_cond_broadcast(&pool->ppool_cond) != 0);
    while (pthread_mutex_unlock(&pool->ppool_lock) != 0);

    return AG_TRUE;
}

/**
 * This function will destroy pthread pool
 * 销毁一个线程池
 *
 * @param pool the pthread pool handle
 *
 * @return None
 */
void ppool_destroy(pool_t *pool)
{
    int i;

    ppool_queue_destroy(pool->queue);

    for (i=0; i < pool->pool_max_num; ++i) {
        pthread_cancel(pool->id[i]);
    }
    free(pool->id);

    pthread_mutex_destroy(&pool->ppool_lock);
    pthread_mutex_destroy(&PPOOL_LOCK);
    pthread_cond_destroy(&pool->ppool_cond);

    free(pool);
}
