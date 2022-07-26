/*
 * Copyright (c) 2006-2020, DreamGrow Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2022-07-26     luhuadong    the first version
 */

#ifndef __AG_LOG_WRAPPER_H__
#define __AG_LOG_WRAPPER_H__

#include "agloo.h"
#include "zlog.h"

int log_init(const char *name);
void log_deinit(void);

#define LOG_I(fmt, arg...)    dzlog_info(fmt, ##arg)

#endif /* __AG_LOG_WRAPPER_H__ */