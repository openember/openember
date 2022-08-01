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

#ifdef AG_LIBS_USING_ZLOG

#define LOG_I(fmt, arg...)    dzlog_info(fmt, ##arg)
#define LOG_D(fmt, arg...)    dzlog_debug(fmt, ##arg)
#define LOG_W(fmt, arg...)    dzlog_warn(fmt, ##arg)
#define LOG_E(fmt, arg...)    dzlog_error(fmt, ##arg)

#else

#define LOG_I(...)               printf(__VA_ARGS__);
#define LOG_D(...)               printf(__VA_ARGS__);
#define LOG_W(...)               printf(__VA_ARGS__);
#define LOG_E(...)               printf(__VA_ARGS__);
#endif

#endif /* __AG_LOG_WRAPPER_H__ */