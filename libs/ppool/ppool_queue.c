/*
 * Copyright (c) 2017-2022, DreamGrow Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2022-11-02     briskgreen   the first version
 */

#include "ppool_queue.h"
#include "ppool_errno.h"

int ppool_queue_get_insert_pos(pool_node *head,int priority);
//查询插入的位置

pool_w *ppool_queue_init(void)
{
	pool_w *head;

	head=malloc(sizeof(pool_w));
	if(!head)
	{
		ppool_errno=PE_QUEUE_NO_MEM;
		return NULL;
	}

	head->len=0;
	head->head=NULL;

	return head;
}

pool_node *ppool_queue_new(ppool_work task, void *arg, int priority)
{
	pool_node *node;

	if(priority < 0)
	{
		ppool_errno=PE_PRIORITY_ERROR;
		return NULL;
	}

	node=malloc(sizeof(pool_node));
	if(node == NULL)
	{
		ppool_errno=PE_QUEUE_NODE_NO_MEM;
		return NULL;
	}

	node->task=task;
	node->arg=arg;
	node->priority=priority;
	node->next=NULL;

	return node;
}

void ppool_queue_add(pool_w *head,pool_node *node)
{
	int pos;
	int i;
	pool_node *h=head->head;

	pos=ppool_queue_get_insert_pos(h,node->priority);
	if(pos == -1)
	{
		head->head=node;
		++head->len;

		return;
	}
	else if(pos == 0)
	{
		node->next=h;
		head->head=node;
		++head->len;

		return;
	}

	for(i=0;i < pos-1;++i)
		h=h->next;

	node->next=h->next;
	h->next=node;
	++head->len;
}

pool_node *ppool_queue_get_task(pool_w *head)
{
	pool_node *task;

	if(head->head == NULL)
		return NULL;

	task=head->head;
	head->head=head->head->next;
	--head->len;

	return task;
}

void ppool_queue_cleanup(pool_w *head)
{
	pool_node *h=head->head;
	pool_node *temp;

	while(h)
	{
		temp=h;
		h=h->next;

		free(temp);
	}

	head->len=0;
	head->head=NULL;
}

void ppool_queue_destroy(pool_w *head)
{
	pool_node *h=head->head;
	pool_node *temp;

	while(h)
	{
		temp=h;
		h=h->next;

		free(temp);
	}

	free(head);
}

int ppool_queue_get_insert_pos(pool_node *head,int priority)
{
	int pos;

	if(head == NULL)
		return -1;

	for(pos=0;head && priority >= head->priority;++pos)
		head=head->next;

	return pos;
}
