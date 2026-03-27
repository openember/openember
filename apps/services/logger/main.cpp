/*
 * logger service v1:
 * - collect logs from files (*.log) under OPENEMBER_LOGGER_LOG_DIR
 * - provide local HTTP API for web_dashboard:
 *   GET /api/logs?limit=200&level=info&tag=foo
 */
#include <mongoose.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#define MODULE_NAME "logger"
#define LOG_TAG MODULE_NAME
#include "openember.h"

static const char *s_root = ".";
static int s_signo = 0;
static std::string s_log_dir =
#ifdef OPENEMBER_LOGGER_LOG_DIR
    OPENEMBER_LOGGER_LOG_DIR
#else
    "/var/log/openember"
#endif
    ;

static void signal_handler(int signo) { s_signo = signo; }

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
                snprintf(tmp, sizeof(tmp), "\\u%04x", (unsigned)(unsigned char)ch);
                out += tmp;
            } else {
                out += ch;
            }
            break;
        }
    }
    return out;
}

static std::string to_lower(std::string s)
{
    for (char &c : s) c = (char)std::tolower((unsigned char)c);
    return s;
}

struct LogItem {
    std::string file;
    std::string line;
};

static bool match_line(const std::string &line, const std::string &level, const std::string &tag)
{
    if (!level.empty()) {
        std::string lv = "[" + to_lower(level) + "]";
        std::string ll = to_lower(line);
        if (ll.find(lv) == std::string::npos) return false;
    }
    if (!tag.empty()) {
        std::string ll = to_lower(line);
        std::string tt = to_lower(tag);
        if (ll.find(tt) == std::string::npos) return false;
    }
    return true;
}

static std::vector<LogItem> collect_logs(const std::string &dir, size_t limit, const std::string &level, const std::string &tag)
{
    std::vector<LogItem> all;
    namespace fs = std::filesystem;
    std::error_code ec;
    if (!fs::exists(dir, ec) || !fs::is_directory(dir, ec)) {
        return all;
    }

    std::vector<fs::path> logs;
    for (auto const &e : fs::directory_iterator(dir, ec)) {
        if (ec) break;
        if (!e.is_regular_file()) continue;
        auto p = e.path();
        if (p.extension() == ".log") logs.push_back(p);
    }
    std::sort(logs.begin(), logs.end());

    const size_t tail_budget = std::max<size_t>(limit * 3, 300);
    for (auto const &p : logs) {
        std::ifstream ifs(p);
        if (!ifs.is_open()) continue;
        std::deque<std::string> tail;
        std::string line;
        while (std::getline(ifs, line)) {
            if (!match_line(line, level, tag)) continue;
            tail.push_back(line);
            if (tail.size() > tail_budget) tail.pop_front();
        }
        for (auto &ln : tail) {
            all.push_back({p.filename().string(), ln});
        }
    }

    if (all.size() > limit) {
        all.erase(all.begin(), all.end() - (long)limit);
    }
    return all;
}

static std::string logs_json(size_t limit, const std::string &level, const std::string &tag)
{
    auto items = collect_logs(s_log_dir, limit, level, tag);
    std::string out = "{";
    out += "\"dir\":\"" + json_escape(s_log_dir) + "\",";
    out += "\"count\":" + std::to_string(items.size()) + ",";
    out += "\"items\":[";
    for (size_t i = 0; i < items.size(); ++i) {
        if (i) out += ",";
        out += "{";
        out += "\"source\":\"" + json_escape(items[i].file) + "\",";
        out += "\"line\":\"" + json_escape(items[i].line) + "\"";
        out += "}";
    }
    out += "]}";
    return out;
}

static void fn(struct mg_connection *c, int ev, void *ev_data)
{
    if (ev != MG_EV_HTTP_MSG) return;
    auto *hm = (struct mg_http_message *)ev_data;

    if (mg_strcmp(hm->uri, mg_str("/health")) == 0) {
        mg_http_reply(c, 200, "Content-Type: text/plain\r\n", "ok\n");
        return;
    }
    if (mg_strcmp(hm->uri, mg_str("/api/logs")) == 0) {
        char b_limit[32] = {0};
        char b_level[64] = {0};
        char b_tag[128] = {0};
        mg_http_get_var(&hm->query, "limit", b_limit, sizeof(b_limit));
        mg_http_get_var(&hm->query, "level", b_level, sizeof(b_level));
        mg_http_get_var(&hm->query, "tag", b_tag, sizeof(b_tag));
        size_t limit = 200;
        if (b_limit[0]) {
            long v = strtol(b_limit, nullptr, 10);
            if (v > 0 && v < 5000) limit = (size_t)v;
        }
        std::string body = logs_json(limit, b_level, b_tag);
        mg_http_reply(c, 200, "Content-Type: application/json\r\nCache-Control: no-store\r\n", "%s", body.c_str());
        return;
    }

    struct mg_http_serve_opts opts = {.root_dir = s_root};
    mg_http_serve_dir(c, hm, &opts);
}

int main(void)
{
    oe_log_init(MODULE_NAME);
    LOG_I("logger service start, scan dir=%s", s_log_dir.c_str());

    unsigned port =
#ifdef OPENEMBER_LOGGER_PORT
        (unsigned)OPENEMBER_LOGGER_PORT;
#else
        18081u;
#endif
    char addr[64];
    snprintf(addr, sizeof(addr), "127.0.0.1:%u", port);

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    struct mg_mgr mgr;
    mg_mgr_init(&mgr);
    if (mg_http_listen(&mgr, addr, fn, nullptr) == nullptr) {
        LOG_E("logger listen failed on %s", addr);
        mg_mgr_free(&mgr);
        return 1;
    }
    LOG_I("logger listening at %s", addr);

    while (s_signo == 0) {
        mg_mgr_poll(&mgr, 1000);
    }
    mg_mgr_free(&mgr);
    return 0;
}

