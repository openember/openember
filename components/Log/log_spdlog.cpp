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
#include <spdlog/sinks/base_sink.h>

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <mutex>
#include <memory>
#include <string>
#include <string_view>
#include <chrono>

#include "transport_backend.hpp"
#include <unistd.h>

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

#ifndef OPENEMBER_SPDLOG_ENABLE_TOPIC
#define OPENEMBER_SPDLOG_ENABLE_TOPIC 0
#endif
#ifndef OPENEMBER_SPDLOG_TOPIC_NAME
#define OPENEMBER_SPDLOG_TOPIC_NAME "/openember/log"
#endif
#ifndef OPENEMBER_SPDLOG_TOPIC_PUB_URL
#if defined(EMBER_LIBS_USING_LCM)
#define OPENEMBER_SPDLOG_TOPIC_PUB_URL "udpm://239.255.76.67:7667?ttl=1"
#else
#define OPENEMBER_SPDLOG_TOPIC_PUB_URL "tcp://*:7561"
#endif
#endif
#ifndef OPENEMBER_SPDLOG_TOPIC_LEVEL
#define OPENEMBER_SPDLOG_TOPIC_LEVEL "info"
#endif
#ifndef OPENEMBER_SPDLOG_TOPIC_RATE_LIMIT
#define OPENEMBER_SPDLOG_TOPIC_RATE_LIMIT 0
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

static std::string json_escape(std::string_view v)
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
            if ((unsigned char)ch < 0x20) {
                char tmp[8];
                std::snprintf(tmp, sizeof(tmp), "\\u%04x", (unsigned)(unsigned char)ch);
                out += tmp;
            } else {
                out += ch;
            }
            break;
        }
    }
    return out;
}

/* LCM only accepts providers like udpm/file; a stale tcp:// URL causes lcm_create to print
 * "Error: LCM provider \"tcp\" not found" on stderr while the main msgbus may still use udpm. */
static std::string normalize_topic_pub_url(const char *raw)
{
    std::string u = raw ? raw : "";
#if defined(EMBER_LIBS_USING_LCM)
    if (u.size() >= 6u && u.compare(0u, 6u, "tcp://") == 0) {
        return "udpm://239.255.76.67:7667?ttl=1";
    }
#endif
    return u;
}

class oe_topic_sink final : public spdlog::sinks::base_sink<std::mutex> {
public:
    oe_topic_sink(const char *pub_url, const char *topic, spdlog::level::level_enum threshold, int rate_limit_lps)
        : pub_url_(normalize_topic_pub_url(pub_url)), topic_(topic ? topic : ""), threshold_(threshold),
          rate_limit_lps_(rate_limit_lps)
    {
    }

    ~oe_topic_sink() override
    {
        if (transport_) {
            (void)transport_->deinit();
            transport_.reset();
        }
    }

protected:
    void sink_it_(const spdlog::details::log_msg &msg) override
    {
        if (msg.level < threshold_) return;

        if (rate_limit_lps_ > 0) {
            using clock = std::chrono::steady_clock;
            const auto now = clock::now();
            if (now - rl_window_start_ >= std::chrono::seconds(1)) {
                rl_window_start_ = now;
                rl_count_ = 0;
            }
            if (rl_count_ >= rate_limit_lps_) {
                return;
            }
            rl_count_++;
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

        // Payload: single-line JSON for cross-language consumers.
        // Note: msg.payload is already formatted string from spdlog logger call.
        const auto ts = std::chrono::duration_cast<std::chrono::milliseconds>(msg.time.time_since_epoch()).count();
        const int pid = (int)getpid();
        std::string payload;
        payload.reserve(msg.payload.size() + 128);
        payload += "{";
        payload += "\"ts_ms\":" + std::to_string((long long)ts) + ",";
        const auto lvl_sv = spdlog::level::to_string_view(msg.level);
        payload += "\"lvl\":\"" + std::string(lvl_sv.data(), lvl_sv.size()) + "\",";
        payload += "\"pid\":" + std::to_string(pid) + ",";
        payload += "\"proc\":\"" + json_escape(proc_) + "\",";
        payload += "\"msg\":\"" + json_escape(std::string_view(msg.payload.data(), msg.payload.size())) + "\"";
        payload += "}";

        (void)transport_->publish_raw(topic_, payload.data(), payload.size());
    }

    void flush_() override {}

public:
    void set_process_name(std::string_view n) { proc_ = std::string(n); }

private:
    std::string pub_url_;
    std::string topic_;
    spdlog::level::level_enum threshold_{spdlog::level::info};
    int rate_limit_lps_{0};

    std::unique_ptr<openember::msgbus::TransportBackend> transport_;
    std::string proc_{"openember"};

    // rate limit state (protected by base_sink mutex)
    std::chrono::steady_clock::time_point rl_window_start_{};
    int rl_count_{0};
};

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

#if OPENEMBER_SPDLOG_ENABLE_TOPIC
        try {
            auto threshold = em_level_from(OPENEMBER_SPDLOG_TOPIC_LEVEL, spdlog::level::info);
            int rl = (int)OPENEMBER_SPDLOG_TOPIC_RATE_LIMIT;
            auto ts = std::make_shared<oe_topic_sink>(OPENEMBER_SPDLOG_TOPIC_PUB_URL, OPENEMBER_SPDLOG_TOPIC_NAME, threshold, rl);
            ts->set_process_name(n);
            sinks.push_back(ts);
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
