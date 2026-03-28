/*
 * Copyright (c) 2022-2023, OpenEmber Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2022-07-21     luhuadong    the first version
 */

#include <mongoose.h>

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <deque>
#include <mutex>
#include <vector>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#include <sys/utsname.h>

#define MODULE_NAME            "web_dashboard"
#define LOG_TAG                MODULE_NAME
#include "openember.h"

static msg_node_t client;

static std::string json_escape(std::string_view v);

static const char *s_debug_level = "2";    // debug level, from 0 to 4
static const char *s_root_dir =
#ifdef OPENEMBER_WEB_DASHBOARD_ROOT_DIR
    OPENEMBER_WEB_DASHBOARD_ROOT_DIR;
#else
    "apps/services/web_dashboard/web_root";
#endif
static const char *s_enable_hexdump = "no";
static const char *s_ssi_pattern = "#.html";
static unsigned s_logger_port =
#ifdef OPENEMBER_LOGGER_PORT
    (unsigned)OPENEMBER_LOGGER_PORT;
#else
    18081u;
#endif
    ;

static std::mutex s_log_mu;
static std::deque<std::string> s_log_ring;
static size_t s_log_ring_max = 2000;

static void on_log_topic_payload(const void *payload, size_t payload_len)
{
    if (!payload || payload_len == 0) return;
    std::string line((const char *)payload, (const char *)payload + payload_len);
    std::lock_guard<std::mutex> lk(s_log_mu);
    s_log_ring.push_back(std::move(line));
    while (s_log_ring.size() > s_log_ring_max) s_log_ring.pop_front();
}

static std::string logs_ring_json(size_t limit)
{
    std::lock_guard<std::mutex> lk(s_log_mu);
    if (limit == 0) limit = 200;
    if (limit > 5000) limit = 5000;
    size_t n = s_log_ring.size();
    size_t start = (n > limit) ? (n - limit) : 0;

    std::string out = "{";
    out += "\"count\":" + std::to_string(n - start) + ",";
    out += "\"items\":[";
    bool first = true;
    for (size_t i = start; i < n; ++i) {
        if (!first) out += ",";
        first = false;
        out += "{\"source\":\"topic\",\"line\":\"";
        out += json_escape(s_log_ring[i]);
        out += "\"}";
    }
    out += "]}";
    return out;
}

static bool local_http_get_json(unsigned port, const std::string &path_qs, std::string &out_json)
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return false;

    struct sockaddr_in addr {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons((uint16_t)port);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
        close(fd);
        return false;
    }

    std::string req = "GET " + path_qs + " HTTP/1.1\r\nHost: 127.0.0.1\r\nConnection: close\r\n\r\n";
    if (send(fd, req.data(), req.size(), 0) < 0) {
        close(fd);
        return false;
    }

    std::string resp;
    char buf[2048];
    for (;;) {
        ssize_t n = recv(fd, buf, sizeof(buf), 0);
        if (n <= 0) break;
        resp.append(buf, buf + n);
    }
    close(fd);

    size_t p = resp.find("\r\n\r\n");
    if (p == std::string::npos) return false;
    std::string head = resp.substr(0, p);
    if (head.find(" 200 ") == std::string::npos) return false;
    out_json = resp.substr(p + 4);
    return true;
}

static std::string read_file_trim(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f) {
        return {};
    }
    std::string s;
    char buf[256];
    size_t n = 0;
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0) {
        s.append(buf, buf + n);
    }
    fclose(f);

    while (!s.empty() && (s.back() == '\n' || s.back() == '\r' || s.back() == ' ' || s.back() == '\t')) {
        s.pop_back();
    }
    size_t i = 0;
    while (i < s.size() && (s[i] == ' ' || s[i] == '\t' || s[i] == '\n' || s[i] == '\r')) {
        i++;
    }
    if (i > 0) {
        s.erase(0, i);
    }
    return s;
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
            if (static_cast<unsigned char>(ch) < 0x20) {
                char tmp[8];
                snprintf(tmp, sizeof(tmp), "\\u%04x", (unsigned)static_cast<unsigned char>(ch));
                out += tmp;
            } else {
                out += ch;
            }
            break;
        }
    }
    return out;
}

static std::string os_pretty_name()
{
    FILE *f = fopen("/etc/os-release", "rb");
    if (!f) {
        return {};
    }
    char line[512];
    std::string pretty;
    while (fgets(line, (int)sizeof(line), f)) {
        const char *k = "PRETTY_NAME=";
        if (strncmp(line, k, strlen(k)) == 0) {
            std::string v = line + strlen(k);
            while (!v.empty() && (v.back() == '\n' || v.back() == '\r')) v.pop_back();
            if (!v.empty() && v.front() == '"') v.erase(0, 1);
            if (!v.empty() && v.back() == '"') v.pop_back();
            pretty = v;
            break;
        }
    }
    fclose(f);
    return pretty;
}

static std::string uptime_pretty()
{
    FILE *f = fopen("/proc/uptime", "rb");
    if (!f) return {};
    double up = 0.0;
    (void)fscanf(f, "%lf", &up);
    fclose(f);
    long sec = (long)up;
    long days = sec / 86400;
    sec %= 86400;
    long hours = sec / 3600;
    sec %= 3600;
    long mins = sec / 60;
    char buf[128];
    if (days > 0) {
        snprintf(buf, sizeof(buf), "%ld days, %ld hrs, %ld mins", days, hours, mins);
    } else if (hours > 0) {
        snprintf(buf, sizeof(buf), "%ld hrs, %ld mins", hours, mins);
    } else {
        snprintf(buf, sizeof(buf), "%ld mins", mins);
    }
    return buf;
}

static bool mem_info_mib(long &used_mib, long &total_mib)
{
    FILE *f = fopen("/proc/meminfo", "rb");
    if (!f) return false;
    long mem_total_kib = 0;
    long mem_avail_kib = 0;
    char key[64];
    long val = 0;
    char unit[32];
    while (fscanf(f, "%63s %ld %31s", key, &val, unit) == 3) {
        if (strcmp(key, "MemTotal:") == 0) mem_total_kib = val;
        if (strcmp(key, "MemAvailable:") == 0) mem_avail_kib = val;
        if (mem_total_kib && mem_avail_kib) break;
    }
    fclose(f);
    if (!mem_total_kib) return false;
    long used_kib = mem_total_kib - mem_avail_kib;
    used_mib = used_kib / 1024;
    total_mib = mem_total_kib / 1024;
    return true;
}

static std::string cpu_pretty()
{
    FILE *f = fopen("/proc/cpuinfo", "rb");
    if (!f) return {};
    char line[512];
    std::string model;
    int processors = 0;
    while (fgets(line, (int)sizeof(line), f)) {
        if (strncmp(line, "model name", 10) == 0) {
            const char *p = strchr(line, ':');
            if (p) {
                model = p + 1;
                while (!model.empty() && (model.back() == '\n' || model.back() == '\r')) model.pop_back();
                while (!model.empty() && (model.front() == ' ' || model.front() == '\t')) model.erase(0, 1);
            }
        } else if (strncmp(line, "processor", 9) == 0) {
            processors++;
        }
    }
    fclose(f);

    // Best-effort max frequency
    std::string mhz_str;
    std::string max_khz = read_file_trim("/sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_max_freq");
    if (!max_khz.empty()) {
        long khz = strtol(max_khz.c_str(), nullptr, 10);
        if (khz > 0) {
            char b[64];
            snprintf(b, sizeof(b), "@ %.3fGHz", (double)khz / 1000000.0);
            mhz_str = b;
        }
    }

    if (model.empty()) return {};
    char buf[512];
    if (processors > 0) {
        snprintf(buf, sizeof(buf), "%s (%d) %s", model.c_str(), processors, mhz_str.c_str());
    } else {
        snprintf(buf, sizeof(buf), "%s %s", model.c_str(), mhz_str.c_str());
    }
    return buf;
}

static std::vector<std::string> gpu_list()
{
    // Best-effort: list PCI display controllers via sysfs, without external tools.
    std::vector<std::string> out;
    // We can't easily enumerate directories without dirent here; use glob via popen is not desired.
    // Provide a minimal fallback from lspci if available is intentionally avoided.
    // Return empty -> frontend can show "unknown".
    return out;
}

static std::string host_product()
{
    std::string vendor = read_file_trim("/sys/class/dmi/id/sys_vendor");
    std::string name = read_file_trim("/sys/class/dmi/id/product_name");
    std::string ver = read_file_trim("/sys/class/dmi/id/product_version");
    std::string s;
    if (!vendor.empty()) s += vendor;
    if (!name.empty()) {
        if (!s.empty()) s += " ";
        s += name;
    }
    if (!ver.empty() && ver != "None") {
        if (!s.empty()) s += " ";
        s += ver;
    }
    return s;
}

static std::string system_info_json()
{
    struct utsname u {};
    std::string kernel;
    std::string arch;
    if (uname(&u) == 0) {
        kernel = u.release;
        arch = u.machine;
    }

    std::string os = os_pretty_name();
    std::string host = host_product();
    std::string uptime = uptime_pretty();
    std::string cpu = cpu_pretty();
    long used_mib = 0, total_mib = 0;
    std::string mem;
    if (mem_info_mib(used_mib, total_mib)) {
        char b[128];
        snprintf(b, sizeof(b), "%ldMiB / %ldMiB", used_mib, total_mib);
        mem = b;
    }

    std::string json = "{";
    json += "\"os\":\"" + json_escape(os) + "\",";
    json += "\"arch\":\"" + json_escape(arch) + "\",";
    json += "\"host\":\"" + json_escape(host) + "\",";
    json += "\"kernel\":\"" + json_escape(kernel) + "\",";
    json += "\"uptime\":\"" + json_escape(uptime) + "\",";
    json += "\"cpu\":\"" + json_escape(cpu) + "\",";
    json += "\"memory\":\"" + json_escape(mem) + "\"";
    json += "}";
    return json;
}

// Handle interrupts, like Ctrl-C
static int s_signo;
static void signal_handler(int signo)
{
    s_signo = signo;
}

static void fn(struct mg_connection *c, int ev, void *ev_data)
{
    struct mg_http_serve_opts opts = {.root_dir = s_root_dir};   // Serve local dir

    if (ev == MG_EV_HTTP_MSG) {
        struct mg_http_message *hm = (struct mg_http_message *)ev_data;
        if (mg_strcmp(hm->uri, mg_str("/api/system")) == 0) {
            std::string body = system_info_json();
            mg_http_reply(c,
                          200,
                          "Content-Type: application/json\r\nCache-Control: no-store\r\n",
                          "%s",
                          body.c_str());
            return;
        }
        if (mg_strcmp(hm->uri, mg_str("/api/logs")) == 0) {
#if defined(OPENEMBER_SPDLOG_ENABLE_TOPIC) && OPENEMBER_SPDLOG_ENABLE_TOPIC
            char b_limit[32] = {0};
            mg_http_get_var(&hm->query, "limit", b_limit, sizeof(b_limit));
            size_t limit = 200;
            if (b_limit[0]) {
                long v = strtol(b_limit, nullptr, 10);
                if (v > 0) limit = (size_t)v;
            }
            std::string body = logs_ring_json(limit);
            mg_http_reply(c, 200, "Content-Type: application/json\r\nCache-Control: no-store\r\n", "%s", body.c_str());
            return;
#else
            std::string path_qs = "/api/logs";
            if (hm->query.len > 0) {
                path_qs += "?";
                path_qs.append(hm->query.buf, hm->query.buf + hm->query.len);
            }
            std::string body;
            if (local_http_get_json(s_logger_port, path_qs, body)) {
                mg_http_reply(c, 200, "Content-Type: application/json\r\nCache-Control: no-store\r\n", "%s", body.c_str());
            } else {
                mg_http_reply(c, 502, "Content-Type: application/json\r\nCache-Control: no-store\r\n",
                              "{\"error\":\"logger_unavailable\",\"hint\":\"start services/logger\"}");
            }
            return;
#endif
        }
        mg_http_serve_dir(c, hm, &opts);
    }
}

static void _msg_arrived_cb(char *topic, void *payload, size_t payloadlen)
{
#if defined(OPENEMBER_SPDLOG_ENABLE_TOPIC) && OPENEMBER_SPDLOG_ENABLE_TOPIC && defined(OPENEMBER_SPDLOG_TOPIC_NAME)
    if (topic && payload && payloadlen > 0) {
        const size_t tlen = std::strlen(OPENEMBER_SPDLOG_TOPIC_NAME);
        if (std::strncmp(topic, OPENEMBER_SPDLOG_TOPIC_NAME, tlen) == 0) {
            on_log_topic_payload(payload, payloadlen);
            return;
        }
    }
#else
    (void)payloadlen;
#endif
    LOG_D("[%s] %s\n", topic, (char *)payload);
}

static int msg_init(void)
{
    int rc = 0, cn = 0;

    rc = msg_bus_init(&client, MODULE_NAME, NULL, _msg_arrived_cb);
    if (rc != EMBER_EOK) {
        LOG_E("Message bus init failed.");
        return -1;
    }

    /* Subscription list */
    rc = msg_bus_subscribe(client, TEST_TOPIC);
    if (rc != EMBER_EOK) cn++;
    rc = msg_bus_subscribe(client, SYS_EVENT_REPLY_TOPIC);
    if (rc != EMBER_EOK) cn++;
    rc = msg_bus_subscribe(client, MOD_REGISTER_REPLY_TOPIC);
    if (rc != EMBER_EOK) cn++;

#if defined(OPENEMBER_SPDLOG_ENABLE_TOPIC) && OPENEMBER_SPDLOG_ENABLE_TOPIC
    rc = msg_bus_subscribe(client, OPENEMBER_SPDLOG_TOPIC_NAME);
    if (rc != EMBER_EOK) cn++;
#endif

    if (cn != 0) {
        msg_bus_deinit(client);
        LOG_E("Message bus subscribe failed.");
        return -EMBER_ERROR;
    }

    return EMBER_EOK;
}

int main()
{
    int rc;
    struct mg_mgr mgr;
    char listening_addr[64];
    unsigned port = 8000;
#ifdef OPENEMBER_WEB_DASHBOARD_PORT
    port = (unsigned)OPENEMBER_WEB_DASHBOARD_PORT;
#endif
    (void)snprintf(listening_addr, sizeof(listening_addr), "0.0.0.0:%u", port);
    const char *s_listening_address = listening_addr;

    sayHello(MODULE_NAME);
    
    log_init(MODULE_NAME);
    LOG_I("Version: %lu.%lu.%lu", EMBER_VERSION, EMBER_SUBVERSION, EMBER_REVISION);

    rc = msg_init();
    if (rc != EMBER_EOK) {
        LOG_E("Message channel init failed.");
        exit(1);
    }

    rc = msg_smm_register(client, MODULE_NAME, SUBMODULE_CLASS_WEB);
    if (rc != EMBER_EOK) {
        LOG_E("Module register failed.");
        exit(1);
    }

#if defined(OPENEMBER_SPDLOG_ENABLE_TOPIC) && OPENEMBER_SPDLOG_ENABLE_TOPIC
    LOG_I("log topic subscribed via msgbus: topic=%s (same transport as framework bus)", OPENEMBER_SPDLOG_TOPIC_NAME);
#endif

    /* Run web server */
    mg_mgr_init(&mgr);

    /* Create listening connection */
    if (NULL == mg_http_listen(&mgr, s_listening_address, fn, NULL)) {
        LOG_E("Exit.");
        mg_mgr_free(&mgr);
        exit(1);
    }

    // Start infinite event loop
    MG_INFO(("Mongoose version : v%s", MG_VERSION));
    MG_INFO(("Listening on     : %s", s_listening_address));
    MG_INFO(("Web root         : [%s]", s_root_dir));

    while (s_signo == 0) {
        mg_mgr_poll(&mgr, 1000);    // Block forever
    }

    mg_mgr_free(&mgr);
    MG_INFO(("Exiting on signal %d", s_signo));
    return 0;
}
