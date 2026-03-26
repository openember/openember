/*
 * Copyright (c) 2022-2023, OpenEmber Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2022-07-07     luhuadong    the first version
 */

#ifndef __OPENEMBER_H__
#define __OPENEMBER_H__

#include "ember_config.h"
#include "ember_def.h"
#include "topic.h"

#include "list.h"

#include "openember/log.h"
#include "log_wrapper.h" /* legacy include: prefer openember/log.h */
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

#endif /* __OPENEMBER_H__ */