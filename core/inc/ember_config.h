/*
 * Copyright (c) 2022-2026, OpenEmber Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2022-07-07     luhuadong    the first version
 * 2026-07-25     openember    remove msgbus backend macros; Link uses Zenoh
 */

#ifndef __EMBER_CONFIG_H__
#define __EMBER_CONFIG_H__


#define EMBER_NAME_MAX         16
#define EMBER_USING_LIBC

/* Log definitions — 后端由 CMake OPENEMBER_LOG_BACKEND 生成 ember_log_backend.h */
#define EMBER_LIBS_USING_LOG

#include "ember_log_backend.h"

/* JSON：nlohmann/json（ember_json_config.h） */
#include "ember_json_config.h"

#if EMBER_LOG_BACKEND_IS_SPDLOG
#define EMBER_LIBS_USING_SPDLOG
#elif EMBER_LOG_BACKEND_IS_BUILTIN
#define EMBER_LIBS_USING_LOG_BUILTIN
#endif

//#define EMBER_LIBS_USING_EASYLOGGER

#endif /* __EMBER_CONFIG_H__ */
