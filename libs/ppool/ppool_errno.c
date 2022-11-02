/*
 * Copyright (c) 2017-2022, DreamGrow Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2022-11-02     briskgreen   the first version
 */

#include "ppool_errno.h"

int ppool_errno=0;

void ppool_error(const char *msg)
{
	if(!msg)
		printf("%s\n",ppool_strerr(ppool_errno));
	else
		printf("%s : %s\n",ppool_strerr(ppool_errno), msg);
}

char *ppool_strerr(int errno)
{
	switch(errno)
	{
		case 0:
			return "成功!";
		case -1:
			return "无法为线程池开辟空间，创建线程池失败!";
		case -2:
			return "无法为此数量的线程分配足够的内存!";
		case -3:
			return "pthread初始化互斥锁失败，请使用ppool_error查看更多信息!";
		case -4:
			return "pthread初始化条件变量失败，请使用ppool_error查看更多信息!";
		case -5:
			return "无法为任务队列开辟空间!";
		case -6:
			return "错误的优先级!";
		case -7:
			return "无法为队列丙创建一个结点，开辟内存出错!";
		default:
			return "未知错误!";
	}
}
