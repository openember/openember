/*
 * Copyright (c) 2017-2022, DreamGrow Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2022-11-02     briskgreen   the first version
 */

#ifndef _PPOOL_ERRNO_H
#define _PPOOL_ERRNO_H

#include <stdio.h>
#include <string.h>

extern int ppool_errno; //错误代码变量

#define PE_OK                  0
#define PE_POOL_NO_MEM        -1
#define PE_THREAD_NO_MEM      -2
#define PE_THREAD_MUTEX_ERROR -3
#define PE_THREAD_COND_ERROR  -4
#define PE_QUEUE_NO_MEM       -5
#define PE_PRIORITY_ERROR     -6
#define PE_QUEUE_NODE_NO_MEM  -7

//打印错误信息
void ppool_error(const char *msg);

//根据错误代码代码打印错误信息
char *ppool_strerr(int errno);

#endif
