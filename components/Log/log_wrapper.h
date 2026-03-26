/*
 * Copyright (c) 2022-2023, OpenEmber Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2022-07-26     luhuadong    the first version
 */

#ifndef __EMBER_LOG_WRAPPER_H__
#define __EMBER_LOG_WRAPPER_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "ember_config.h"

int log_init(const char *name);
void log_deinit(void);

/*
 * Legacy header:
 * - Keep log_init/log_deinit for existing apps
 * - LOG_* macros are now provided by openember/log.h (included via openember.h)
 */

#ifdef __cplusplus
}
#endif

#endif /* __EMBER_LOG_WRAPPER_H__ */
