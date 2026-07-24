/*
 * Copyright (c) 2026, OpenEmber Team
 * SPDX-License-Identifier: Apache-2.0
 */

#include "openember/logging/logger.hpp"

#include "ember_config.h"

#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

#if defined(EMBER_LIBS_USING_SPDLOG)
#include <spdlog/spdlog.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/syslog_sink.h>
#include <spdlog/sinks/base_sink.h>

#include "transport_backend.hpp"
#include <chrono>
#include <filesystem>
#include <unistd.h>
#endif

namespace openember::logging {
namespace {

std::mutex              g_mu;
std::unique_ptr<Logger> g_default;
bool                    g_initialized = false;

#if defined(EMBER_LIBS_USING_SPDLOG)
spdlog::level::level_enum parse_level(std::string_view s, spdlog::level::level_enum def)
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

std::string json_escape(std::string_view v)
{
    std::string out;
    out.reserve(v.size() + 16);
    for (char ch : v) {
        switch (ch) {
        case '\\': out += "\\\\"; break;
        case '"': out += "\\\""; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        default:
            if (static_cast<unsigned char>(ch) < 0x20) {
                char tmp[8];
                std::snprintf(tmp, sizeof(tmp), "\\u%04x",
                              static_cast<unsigned>(static_cast<unsigned char>(ch)));
                out += tmp;
            } else {
                out += ch;
            }
            break;
        }
    }
    return out;
}

std::string normalize_topic_pub_url(const std::string& raw)
{
#if defined(EMBER_LIBS_USING_LCM)
    if (raw.size() >= 6u && raw.compare(0u, 6u, "tcp://") == 0) {
        return "udpm://239.255.76.67:7667?ttl=1";
    }
#endif
    return raw;
}

class TopicSink final : public spdlog::sinks::base_sink<std::mutex> {
public:
    TopicSink(std::string pub_url, std::string topic, spdlog::level::level_enum threshold, int rate_limit_lps)
        : pub_url_(normalize_topic_pub_url(std::move(pub_url)))
        , topic_(std::move(topic))
        , threshold_(threshold)
        , rate_limit_lps_(rate_limit_lps)
    {
    }

    ~TopicSink() override
    {
        if (transport_) {
            (void)transport_->deinit();
            transport_.reset();
        }
    }

    void set_process_name(std::string_view n) { proc_ = std::string(n); }

protected:
    void sink_it_(const spdlog::details::log_msg& msg) override
    {
        if (msg.level < threshold_) {
            return;
        }

        if (rate_limit_lps_ > 0) {
            using clock = std::chrono::steady_clock;
            const auto now = clock::now();
            if (now - rl_window_start_ >= std::chrono::seconds(1)) {
                rl_window_start_ = now;
                rl_count_        = 0;
            }
            if (rl_count_ >= rate_limit_lps_) {
                return;
            }
            ++rl_count_;
        }

        if (!transport_) {
            transport_ = openember::msgbus::CreateDefaultTransportBackend();
            if (!transport_) {
                return;
            }
            openember::msgbus::MessageCallback no_recv{};
            if (transport_->init("spdlog", pub_url_, std::move(no_recv)) != 0) {
                transport_.reset();
                return;
            }
        }

        const auto ts =
            std::chrono::duration_cast<std::chrono::milliseconds>(msg.time.time_since_epoch()).count();
        const int pid = static_cast<int>(::getpid());
        std::string payload;
        payload.reserve(msg.payload.size() + 128);
        payload += "{";
        payload += "\"ts_ms\":" + std::to_string(static_cast<long long>(ts)) + ",";
        const auto lvl_sv = spdlog::level::to_string_view(msg.level);
        payload += "\"lvl\":\"" + std::string(lvl_sv.data(), lvl_sv.size()) + "\",";
        payload += "\"pid\":" + std::to_string(pid) + ",";
        payload += "\"proc\":\"" + json_escape(proc_) + "\",";
        payload += "\"msg\":\"" + json_escape(std::string_view(msg.payload.data(), msg.payload.size())) + "\"";
        payload += "}";

        (void)transport_->publish_raw(topic_, payload.data(), payload.size());
    }

    void flush_() override {}

private:
    std::string pub_url_;
    std::string topic_;
    spdlog::level::level_enum threshold_{spdlog::level::info};
    int rate_limit_lps_{0};
    std::unique_ptr<openember::msgbus::TransportBackend> transport_;
    std::string proc_{"openember"};
    std::chrono::steady_clock::time_point rl_window_start_{};
    int rl_count_{0};
};
#endif

}  // namespace

LoggerConfig LoggerConfig::from_build_defaults(std::string_view process_name)
{
    LoggerConfig cfg;
    cfg.name = process_name.empty() ? "openember" : std::string(process_name);

#ifdef OPENEMBER_SPDLOG_LEVEL
    cfg.log_level = OPENEMBER_SPDLOG_LEVEL;
#endif
#ifdef OPENEMBER_SPDLOG_FLUSH_LEVEL
    cfg.flush_level = OPENEMBER_SPDLOG_FLUSH_LEVEL;
#endif
#ifdef OPENEMBER_SPDLOG_PATTERN
    cfg.pattern = OPENEMBER_SPDLOG_PATTERN;
#endif
#if defined(OPENEMBER_SPDLOG_ENABLE_STDOUT)
    cfg.enable_stdout = OPENEMBER_SPDLOG_ENABLE_STDOUT != 0;
#endif
#if defined(OPENEMBER_SPDLOG_ENABLE_FILE)
    cfg.enable_file = OPENEMBER_SPDLOG_ENABLE_FILE != 0;
#endif
#if defined(OPENEMBER_SPDLOG_ENABLE_SYSLOG)
    cfg.enable_syslog = OPENEMBER_SPDLOG_ENABLE_SYSLOG != 0;
#endif
#if defined(OPENEMBER_SPDLOG_ENABLE_TOPIC)
    cfg.enable_topic = OPENEMBER_SPDLOG_ENABLE_TOPIC != 0;
#endif
#ifdef OPENEMBER_SPDLOG_FILE_DIR
    cfg.file_dir = OPENEMBER_SPDLOG_FILE_DIR;
#endif
#if defined(OPENEMBER_SPDLOG_ROTATE_MAX_MB)
    cfg.max_file_size_mb = static_cast<std::size_t>(OPENEMBER_SPDLOG_ROTATE_MAX_MB);
#endif
#if defined(OPENEMBER_SPDLOG_ROTATE_MAX_FILES)
    cfg.max_file_count = static_cast<std::size_t>(OPENEMBER_SPDLOG_ROTATE_MAX_FILES);
#endif
#ifdef OPENEMBER_SPDLOG_TOPIC_NAME
    cfg.topic_name = OPENEMBER_SPDLOG_TOPIC_NAME;
#endif
#ifdef OPENEMBER_SPDLOG_TOPIC_PUB_URL
    cfg.topic_pub_url = OPENEMBER_SPDLOG_TOPIC_PUB_URL;
#endif
#ifdef OPENEMBER_SPDLOG_TOPIC_LEVEL
    cfg.topic_level = OPENEMBER_SPDLOG_TOPIC_LEVEL;
#endif
#if defined(OPENEMBER_SPDLOG_TOPIC_RATE_LIMIT)
    cfg.topic_rate_limit = static_cast<int>(OPENEMBER_SPDLOG_TOPIC_RATE_LIMIT);
#endif
    return cfg;
}

struct Logger::Impl {
#if defined(EMBER_LIBS_USING_SPDLOG)
    std::shared_ptr<spdlog::logger> spd;
#else
    std::string name;
#endif
};

Logger::Logger(LoggerConfig config) : config_(std::move(config)), impl_(std::make_unique<Impl>())
{
    if (config_.name.empty()) {
        config_.name = "openember";
    }
    if (config_.max_file_size_mb == 0) {
        config_.max_file_size_mb = 10;
    }
    if (config_.max_file_count == 0) {
        config_.max_file_count = 5;
    }

#if defined(EMBER_LIBS_USING_SPDLOG)
    try {
        std::vector<spdlog::sink_ptr> sinks;

        if (config_.enable_stdout) {
            sinks.push_back(std::make_shared<spdlog::sinks::stdout_color_sink_mt>());
        }

        if (config_.enable_file) {
            try {
                std::filesystem::path dir = config_.file_dir;
                std::error_code ec;
                std::filesystem::create_directories(dir, ec);
                const auto file = dir / (config_.name + ".log");
                const std::size_t max_bytes = config_.max_file_size_mb * 1024u * 1024u;
                sinks.push_back(std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
                    file.string(), max_bytes, config_.max_file_count));
            } catch (...) {
            }
        }

        if (config_.enable_syslog) {
            try {
                sinks.push_back(std::make_shared<spdlog::sinks::syslog_sink_mt>(
                    config_.name, 0, LOG_USER, true));
            } catch (...) {
            }
        }

        if (config_.enable_topic) {
            try {
                auto threshold = parse_level(config_.topic_level, spdlog::level::info);
                auto ts = std::make_shared<TopicSink>(
                    config_.topic_pub_url, config_.topic_name, threshold, config_.topic_rate_limit);
                ts->set_process_name(config_.name);
                sinks.push_back(std::move(ts));
            } catch (...) {
            }
        }

        if (sinks.empty()) {
            sinks.push_back(std::make_shared<spdlog::sinks::stdout_color_sink_mt>());
        }

        impl_->spd = std::make_shared<spdlog::logger>(config_.name, sinks.begin(), sinks.end());
        spdlog::register_logger(impl_->spd);
        spdlog::set_default_logger(impl_->spd);
        impl_->spd->set_level(parse_level(config_.log_level, spdlog::level::info));
        impl_->spd->flush_on(parse_level(config_.flush_level, spdlog::level::info));
        impl_->spd->set_pattern(config_.pattern);
    } catch (...) {
        impl_->spd.reset();
    }
#else
    impl_->name = config_.name;
#endif
}

Logger::~Logger()
{
#if defined(EMBER_LIBS_USING_SPDLOG)
    if (impl_ && impl_->spd) {
        try {
            const auto n = impl_->spd->name();
            impl_->spd->flush();
            spdlog::drop(n);
            impl_->spd.reset();
        } catch (...) {
            impl_->spd.reset();
        }
    }
#endif
}

void Logger::write(Level level, std::string_view tag, std::string_view msg)
{
#if defined(EMBER_LIBS_USING_SPDLOG)
    if (!impl_ || !impl_->spd) {
        return;
    }
    spdlog::level::level_enum lvl = spdlog::level::info;
    switch (level) {
    case Level::Debug: lvl = spdlog::level::debug; break;
    case Level::Info: lvl = spdlog::level::info; break;
    case Level::Warn: lvl = spdlog::level::warn; break;
    case Level::Error:
    default: lvl = spdlog::level::err; break;
    }
    if (!tag.empty()) {
        impl_->spd->log(lvl, "[{}] {}", tag, msg);
    } else {
        impl_->spd->log(lvl, "{}", msg);
    }
#else
    const char *lp = "I";
    switch (level) {
    case Level::Debug: lp = "D"; break;
    case Level::Info: lp = "I"; break;
    case Level::Warn: lp = "W"; break;
    case Level::Error:
    default: lp = "E"; break;
    }
    if (!tag.empty()) {
        std::fprintf(stderr, "[%s] [%s] [%.*s] %.*s\n", lp, config_.name.c_str(),
                     static_cast<int>(tag.size()), tag.data(),
                     static_cast<int>(msg.size()), msg.data());
    } else {
        std::fprintf(stderr, "[%s] [%s] %.*s\n", lp, config_.name.c_str(),
                     static_cast<int>(msg.size()), msg.data());
    }
#endif
}

void Logger::writef(Level level, std::string_view tag, const char *fmt, ...)
{
    char buf[2048];
    va_list ap;
    va_start(ap, fmt);
    std::vsnprintf(buf, sizeof(buf), fmt ? fmt : "", ap);
    va_end(ap);
    write(level, tag, buf);
}

void Logger::debug(std::string_view msg) { write(Level::Debug, {}, msg); }
void Logger::info(std::string_view msg) { write(Level::Info, {}, msg); }
void Logger::warn(std::string_view msg) { write(Level::Warn, {}, msg); }
void Logger::error(std::string_view msg) { write(Level::Error, {}, msg); }

void Logger::debug_tag(std::string_view tag, std::string_view msg) { write(Level::Debug, tag, msg); }
void Logger::info_tag(std::string_view tag, std::string_view msg) { write(Level::Info, tag, msg); }
void Logger::warn_tag(std::string_view tag, std::string_view msg) { write(Level::Warn, tag, msg); }
void Logger::error_tag(std::string_view tag, std::string_view msg) { write(Level::Error, tag, msg); }

void init(LoggerConfig config)
{
    std::lock_guard lock(g_mu);
    g_default = std::make_unique<Logger>(std::move(config));
    g_initialized = true;
}

void init(std::string_view process_name)
{
    init(LoggerConfig::from_build_defaults(process_name));
}

void deinit()
{
    std::lock_guard lock(g_mu);
    g_default.reset();
    g_initialized = false;
#if defined(EMBER_LIBS_USING_SPDLOG)
    spdlog::shutdown();
#endif
}

Logger& default_logger()
{
    std::lock_guard lock(g_mu);
    if (!g_default) {
        g_default = std::make_unique<Logger>(LoggerConfig::from_build_defaults("openember"));
        g_initialized = true;
    }
    return *g_default;
}

bool is_initialized() noexcept
{
    std::lock_guard lock(g_mu);
    return g_initialized && static_cast<bool>(g_default);
}

}  // namespace openember::logging

#include "openember/logging/log_c.h"

extern "C" int oe_log_init(const char *process_name)
{
    openember::logging::init(process_name ? process_name : "openember");
    return 0;
}

extern "C" void oe_log_deinit(void)
{
    openember::logging::deinit();
}

extern "C" void oe_log_vwrite(oe_log_level_t level, const char *tag, const char *fmt, va_list ap)
{
    char buf[2048];
    std::vsnprintf(buf, sizeof(buf), fmt ? fmt : "", ap);
    openember::logging::Level lvl = openember::logging::Level::Info;
    switch (level) {
    case OE_LOG_LEVEL_DEBUG: lvl = openember::logging::Level::Debug; break;
    case OE_LOG_LEVEL_INFO: lvl = openember::logging::Level::Info; break;
    case OE_LOG_LEVEL_WARN: lvl = openember::logging::Level::Warn; break;
    case OE_LOG_LEVEL_ERROR:
    default: lvl = openember::logging::Level::Error; break;
    }
    openember::logging::default_logger().write(lvl, tag ? tag : "", buf);
}

extern "C" void oe_log_write(oe_log_level_t level, const char *tag, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    oe_log_vwrite(level, tag, fmt, ap);
    va_end(ap);
}

extern "C" int log_init(const char *name)
{
    return oe_log_init(name);
}

extern "C" void log_deinit(void)
{
    oe_log_deinit();
}
