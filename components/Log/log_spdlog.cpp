/*
 * Copyright (c) 2025, OpenEmber Team
 * SPDX-License-Identifier: Apache-2.0
 *
 * spdlog 后端：供 C 代码通过 ember_log_* 调用。
 */
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include <cstdarg>
#include <cstdio>
#include <memory>

static std::shared_ptr<spdlog::logger> g_ember_logger;

extern "C" int log_spdlog_init(const char *name)
{
    try {
        const char *n = (name && name[0]) ? name : "openember";
        g_ember_logger = spdlog::stdout_color_mt(n);
        spdlog::set_default_logger(g_ember_logger);
        spdlog::set_level(spdlog::level::debug);
        spdlog::flush_on(spdlog::level::info);
        spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] %v");
        return 0;
    } catch (...) {
        return -1;
    }
}

extern "C" void log_spdlog_deinit(void)
{
    g_ember_logger.reset();
    spdlog::shutdown();
}

static void em_spdlog_va(spdlog::level::level_enum lvl, const char *fmt, std::va_list ap)
{
    char buf[2048];
    std::vsnprintf(buf, sizeof(buf), fmt, ap);
    if (auto lg = g_ember_logger ? g_ember_logger : spdlog::default_logger()) {
        lg->log(lvl, buf);
    }
}

static void em_spdlog_tag_va(spdlog::level::level_enum lvl, const char *tag, const char *fmt, std::va_list ap)
{
    char buf[2048];
    std::vsnprintf(buf, sizeof(buf), fmt, ap);
    const char *t = (tag && tag[0]) ? tag : "";
    if (auto lg = g_ember_logger ? g_ember_logger : spdlog::default_logger()) {
        if (t[0]) {
            lg->log(lvl, "[{}] {}", t, buf);
        } else {
            lg->log(lvl, "{}", buf);
        }
    }
}

extern "C" void ember_log_debug(const char *fmt, ...)
{
    std::va_list ap;
    va_start(ap, fmt);
    em_spdlog_va(spdlog::level::debug, fmt, ap);
    va_end(ap);
}

extern "C" void ember_log_info(const char *fmt, ...)
{
    std::va_list ap;
    va_start(ap, fmt);
    em_spdlog_va(spdlog::level::info, fmt, ap);
    va_end(ap);
}

extern "C" void ember_log_warn(const char *fmt, ...)
{
    std::va_list ap;
    va_start(ap, fmt);
    em_spdlog_va(spdlog::level::warn, fmt, ap);
    va_end(ap);
}

extern "C" void ember_log_error(const char *fmt, ...)
{
    std::va_list ap;
    va_start(ap, fmt);
    em_spdlog_va(spdlog::level::err, fmt, ap);
    va_end(ap);
}

extern "C" void ember_log_debug_tag(const char *tag, const char *fmt, ...)
{
    std::va_list ap;
    va_start(ap, fmt);
    em_spdlog_tag_va(spdlog::level::debug, tag, fmt, ap);
    va_end(ap);
}

extern "C" void ember_log_info_tag(const char *tag, const char *fmt, ...)
{
    std::va_list ap;
    va_start(ap, fmt);
    em_spdlog_tag_va(spdlog::level::info, tag, fmt, ap);
    va_end(ap);
}

extern "C" void ember_log_warn_tag(const char *tag, const char *fmt, ...)
{
    std::va_list ap;
    va_start(ap, fmt);
    em_spdlog_tag_va(spdlog::level::warn, tag, fmt, ap);
    va_end(ap);
}

extern "C" void ember_log_error_tag(const char *tag, const char *fmt, ...)
{
    std::va_list ap;
    va_start(ap, fmt);
    em_spdlog_tag_va(spdlog::level::err, tag, fmt, ap);
    va_end(ap);
}
