/*
 * Copyright (c) 2017-2022, DreamGrow Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2022-11-02     briskgreen   the first version
 */

#include "ppool.h"
#include "ppool_errno.h"

//线程池执行任务函数
void ppool_run(pool_t *pool);

pool_t *ppool_init(int pool_max_num)
{
	pool_t *pool;
	pool_w *head;
	int i;

	pool=malloc(sizeof(pool_t));
	if(!pool)
	{
		ppool_errno=PE_POOL_NO_MEM;
		return NULL;
	}

	//创建任务队列
	head=ppool_queue_init();
	if(!head)
	{
		free(pool);
		return NULL;
	}

	pool->pool_max_num=pool_max_num;
	pool->rel_num=0;
	pool->head=head;
	pool->id=malloc(sizeof(pthread_t)*pool_max_num);
	if(!pool->id)
	{
		ppool_errno=PE_THREAD_NO_MEM;
		free(head);
		free(pool);

		return NULL;
	}

	if(pthread_mutex_init(&pool->ppool_lock,NULL) != 0)
	{
		ppool_errno=PE_THREAD_MUTEX_ERROR;
		free(pool->id);
		free(head);
		free(pool);

		return NULL;
	}
	if(pthread_mutex_init(&PPOOL_LOCK,NULL) != 0)
	{
		ppool_errno=PE_THREAD_MUTEX_ERROR;
		free(pool->id);
		free(head);
		pthread_mutex_destroy(&pool->ppool_lock);
		free(pool);
	}
	if(pthread_cond_init(&pool->ppool_cond,NULL) != 0)
	{
		ppool_errno=PE_THREAD_COND_ERROR;
		free(pool->id);
		free(head);
		pthread_mutex_destroy(&pool->ppool_lock);
		free(pool);

		return NULL;
	}

	//创建任务
	for(i=0;i < pool_max_num;++i)
	{
		if(pthread_create(&pool->id[i],NULL,(void *)ppool_run,pool) == 0)
			++pool->rel_num;

		pthread_detach(pool->id[i]);
	}

	return pool;
}

pbool ppool_add(pool_t *pool,pool_task *task)
{
	pool_node *node;

	node=ppool_queue_new(task->task,task->arg,task->priority);
	if(!node)
		return PFALSE;

	while(pthread_mutex_lock(&pool->ppool_lock) != 0);
	ppool_queue_add(pool->head,node);
	while(pthread_cond_broadcast(&pool->ppool_cond) != 0);
	while(pthread_mutex_unlock(&pool->ppool_lock) != 0);

	return PTRUE;
}

void ppool_destroy(pool_t *pool)
{
	int i;

	ppool_queue_destroy(pool->head);

	for(i=0;i < pool->pool_max_num;++i)
		pthread_cancel(pool->id[i]);
	free(pool->id);

	pthread_mutex_destroy(&pool->ppool_lock);
	pthread_mutex_destroy(&PPOOL_LOCK);
	pthread_cond_destroy(&pool->ppool_cond);

	free(pool);
}

void ppool_run(pool_t *pool)
{
	pool_node *task;

	while(1)
	{
		while(pthread_mutex_lock(&pool->ppool_lock) != 0);

		while(pool->head->len <= 0) {
			pthread_cond_wait(&pool->ppool_cond,&pool->ppool_lock);
		}
		task=ppool_queue_get_task(pool->head);

		while(pthread_mutex_unlock(&pool->ppool_lock) != 0);

		if(task == NULL) continue;
		task->task(task->arg);

		free(task);
	}
}
