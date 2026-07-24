/*
 * Copyright (c) 2026, OpenEmber Team
 * SPDX-License-Identifier: Apache-2.0
 *
 * Preferred logging facade for OpenEmber apps/modules/components.
 */

#pragma once

#include "openember/logging/logger.hpp"

#ifndef LOG_TAG
#define LOG_TAG ""
#endif

#ifndef OE_LOG_TAG
#define OE_LOG_TAG LOG_TAG
#endif

#define OE_LOGD(...) ::openember::logging::default_logger().writef( \
    ::openember::logging::Level::Debug, OE_LOG_TAG, __VA_ARGS__)
#define OE_LOGI(...) ::openember::logging::default_logger().writef( \
    ::openember::logging::Level::Info, OE_LOG_TAG, __VA_ARGS__)
#define OE_LOGW(...) ::openember::logging::default_logger().writef( \
    ::openember::logging::Level::Warn, OE_LOG_TAG, __VA_ARGS__)
#define OE_LOGE(...) ::openember::logging::default_logger().writef( \
    ::openember::logging::Level::Error, OE_LOG_TAG, __VA_ARGS__)

#ifndef LOG_D
#define LOG_D(...) OE_LOGD(__VA_ARGS__)
#endif
#ifndef LOG_I
#define LOG_I(...) OE_LOGI(__VA_ARGS__)
#endif
#ifndef LOG_W
#define LOG_W(...) OE_LOGW(__VA_ARGS__)
#endif
#ifndef LOG_E
#define LOG_E(...) OE_LOGE(__VA_ARGS__)
#endif

/* Legacy init symbols (defined in logger.cpp). Prefer openember::logging::init(). */
extern "C" int  log_init(const char *name);
extern "C" void log_deinit(void);
extern "C" int  oe_log_init(const char *process_name);
extern "C" void oe_log_deinit(void);
