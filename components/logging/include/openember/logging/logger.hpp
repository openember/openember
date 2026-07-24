/*
 * Copyright (c) 2026, OpenEmber Team
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2026-07-25     openember    C++ Logger + LoggerConfig (spdlog-backed)
 */

#pragma once

#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>

namespace openember::logging {

enum class Level : int {
    Debug = 10,
    Info  = 20,
    Warn  = 30,
    Error = 40,
};

struct LoggerConfig {
    std::string name             = "openember";
    std::string log_level        = "info";
    std::string flush_level      = "info";
    std::string pattern          = "[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] %v";

    bool enable_stdout           = true;
    bool enable_file             = true;
    bool enable_syslog           = false;
    bool enable_topic            = true;

    std::string file_dir         = "/var/log/openember";
    std::size_t max_file_size_mb = 10;
    std::size_t max_file_count   = 5;

    std::string topic_name       = "/openember/log";
    std::string topic_pub_url    = "tcp://*:7561";
    std::string topic_level      = "info";
    int         topic_rate_limit = 0;  // lines/sec, 0 = unlimited

    /** Fill fields from compile-time Kconfig/CMake defaults when available. */
    static LoggerConfig from_build_defaults(std::string_view process_name = "openember");
};

/**
 * Process-oriented logger with optional sinks (stdout / rotating file / syslog / topic).
 * Prefer a single process-wide instance via init() / default_logger().
 */
class Logger {
public:
    explicit Logger(LoggerConfig config);
    ~Logger();

    Logger(const Logger&)            = delete;
    Logger& operator=(const Logger&) = delete;
    Logger(Logger&&)                 = delete;
    Logger& operator=(Logger&&)      = delete;

    void debug(std::string_view msg);
    void info(std::string_view msg);
    void warn(std::string_view msg);
    void error(std::string_view msg);

    void debug_tag(std::string_view tag, std::string_view msg);
    void info_tag(std::string_view tag, std::string_view msg);
    void warn_tag(std::string_view tag, std::string_view msg);
    void error_tag(std::string_view tag, std::string_view msg);

    void write(Level level, std::string_view tag, std::string_view msg);
    void writef(Level level, std::string_view tag, const char *fmt, ...);

    const std::string& name() const noexcept { return config_.name; }
    const LoggerConfig& config() const noexcept { return config_; }

private:
    struct Impl;
    LoggerConfig              config_;
    std::unique_ptr<Impl>     impl_;
};

/** Initialize (or replace) the process-wide default logger. */
void init(LoggerConfig config);
void init(std::string_view process_name);
void deinit();

/** Returns the process-wide logger; creates a default one if needed. */
Logger& default_logger();

bool is_initialized() noexcept;

}  // namespace openember::logging
