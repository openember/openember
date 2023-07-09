/*
 * Copyright (c) 2006-2020, DreamGrow Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2022-07-07     luhuadong    the first version
 */

#ifndef __AGLOO_H__
#define __AGLOO_H__

#include "agconfig.h"
#include "agdef.h"
#include "topic.h"

#include "list.h"

#include "log_wrapper.h"
#include "message.h"
#include "common.h"

#ifdef __cplusplus
extern "C" {
#endif

#define INIT_EXPORT(fn)  void fn (void) __attribute__ ((constructor));
#define EXIT_EXPORT(fn)  void fn (void) __attribute__ ((destructor));

#ifdef __cplusplus
}
#endif

#endif /* __AGLOO_H__ */