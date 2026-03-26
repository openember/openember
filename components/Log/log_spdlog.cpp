/*
 * Copyright (c) 2025, OpenEmber Team
 * SPDX-License-Identifier: Apache-2.0
 *
 * spdlog 后端：供 C 代码通过 ember_log_* 调用。
 */
#include <spdlog/spdlog.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/syslog_sink.h>

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>

#ifndef OPENEMBER_SPDLOG_LEVEL
#define OPENEMBER_SPDLOG_LEVEL "info"
#endif
#ifndef OPENEMBER_SPDLOG_FLUSH_LEVEL
#define OPENEMBER_SPDLOG_FLUSH_LEVEL "info"
#endif
#ifndef OPENEMBER_SPDLOG_PATTERN
#define OPENEMBER_SPDLOG_PATTERN "[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] %v"
#endif
#ifndef OPENEMBER_SPDLOG_ENABLE_STDOUT
#define OPENEMBER_SPDLOG_ENABLE_STDOUT 1
#endif
#ifndef OPENEMBER_SPDLOG_ENABLE_FILE
#define OPENEMBER_SPDLOG_ENABLE_FILE 1
#endif
#ifndef OPENEMBER_SPDLOG_FILE_DIR
#define OPENEMBER_SPDLOG_FILE_DIR "/var/log/openember"
#endif
#ifndef OPENEMBER_SPDLOG_ROTATE_MAX_MB
#define OPENEMBER_SPDLOG_ROTATE_MAX_MB 10
#endif
#ifndef OPENEMBER_SPDLOG_ROTATE_MAX_FILES
#define OPENEMBER_SPDLOG_ROTATE_MAX_FILES 5
#endif
#ifndef OPENEMBER_SPDLOG_ENABLE_SYSLOG
#define OPENEMBER_SPDLOG_ENABLE_SYSLOG 0
#endif

static std::shared_ptr<spdlog::logger> g_ember_logger;

static spdlog::level::level_enum em_level_from(std::string_view s, spdlog::level::level_enum def)
{
    if (s == "trace") return spdlog::level::trace;
    if (s == "debug") return spdlog::level::debug;
    if (s == "info") return spdlog::level::info;
    if (s == "warn" || s == "warning") return spdlog::level::warn;
    if (s == "error" || s == "err") return spdlog::level::err;
    if (s == "critical") return spdlog::level::critical;
    if (s == "off") return spdlog::level::off;
    return def;
}

extern "C" int log_spdlog_init(const char *name)
{
    try {
        const char *n = (name && name[0]) ? name : "openember";
        std::vector<spdlog::sink_ptr> sinks;

#if OPENEMBER_SPDLOG_ENABLE_STDOUT
        sinks.push_back(std::make_shared<spdlog::sinks::stdout_color_sink_mt>());
#endif

#if OPENEMBER_SPDLOG_ENABLE_FILE
        try {
            std::filesystem::path dir = OPENEMBER_SPDLOG_FILE_DIR;
            std::error_code ec;
            std::filesystem::create_directories(dir, ec);

            std::string fname = std::string(n) + ".log";
            std::filesystem::path file = dir / fname;
            const size_t max_bytes = static_cast<size_t>(OPENEMBER_SPDLOG_ROTATE_MAX_MB) * 1024u * 1024u;
            const size_t max_files = static_cast<size_t>(OPENEMBER_SPDLOG_ROTATE_MAX_FILES);
            sinks.push_back(std::make_shared<spdlog::sinks::rotating_file_sink_mt>(file.string(), max_bytes, max_files));
        } catch (...) {
            /* If file sink fails, keep other sinks. */
        }
#endif

#if OPENEMBER_SPDLOG_ENABLE_SYSLOG
        try {
            sinks.push_back(std::make_shared<spdlog::sinks::syslog_sink_mt>(n, 0, LOG_USER, true));
        } catch (...) {
        }
#endif

        if (sinks.empty()) {
            sinks.push_back(std::make_shared<spdlog::sinks::stdout_color_sink_mt>());
        }

        g_ember_logger = std::make_shared<spdlog::logger>(n, sinks.begin(), sinks.end());
        spdlog::register_logger(g_ember_logger);
        spdlog::set_default_logger(g_ember_logger);
        spdlog::set_level(em_level_from(OPENEMBER_SPDLOG_LEVEL, spdlog::level::info));
        spdlog::flush_on(em_level_from(OPENEMBER_SPDLOG_FLUSH_LEVEL, spdlog::level::info));
        spdlog::set_pattern(OPENEMBER_SPDLOG_PATTERN);
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
