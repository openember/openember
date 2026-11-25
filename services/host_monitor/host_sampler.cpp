/*
 * Copyright (c) 2022-2026, OpenEmber Team
 * SPDX-License-Identifier: Apache-2.0
 */
#include "host_sampler.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <ctime>
#include <fstream>
#include <set>
#include <sstream>
#include <sys/statvfs.h>
#include <sys/utsname.h>
#include <unistd.h>

namespace openember::host_monitor {
namespace {
std::string Line(const std::string& path, bool* ok = nullptr) {
    std::ifstream in(path);
    std::string line;
    bool success = bool(std::getline(in, line));
    // Empty sysfs isolated CPU list is a successful observation.
    if (ok) *ok = success;
    return line;
}
std::map<std::string, std::string> OsRelease(const std::string& path) {
    std::map<std::string, std::string> result;
    std::ifstream in(path);
    std::string line;
    while (std::getline(in, line)) {
        auto sep = line.find('=');
        if (sep == std::string::npos || line.empty() || line[0] == '#') continue;
        auto value = line.substr(sep + 1);
        if (value.size() >= 2 && (value.front() == '"' || value.front() == '\'') && value.back() == value.front())
            value = value.substr(1, value.size() - 2);
        result[line.substr(0, sep)] = value;
    }
    return result;
}
MemorySample Memory(const std::string& path) {
    MemorySample out;
    std::ifstream in(path);
    std::map<std::string, std::uint64_t> values;
    std::string line;
    while (std::getline(in, line)) {
        std::istringstream row(line);
        std::string key, unit;
        std::uint64_t value;
        if (row >> key >> value >> unit && unit == "kB" && value <= UINT64_MAX / 1024)
            values[key] = value * 1024;
    }
    if (!values.count("MemTotal:") || values["MemTotal:"] == 0) return out;
    out.total_bytes = values["MemTotal:"];
    if (values.count("MemAvailable:")) {
        out.available_bytes = values["MemAvailable:"];
    } else {
        if (!values.count("MemFree:") || !values.count("Buffers:") || !values.count("Cached:")) return out;
        out.available_estimated = true;
        // Saturate each addition rather than overflowing malformed counters.
        for (const auto* key : {"MemFree:", "Buffers:", "Cached:"})
            out.available_bytes += std::min(values[key], out.total_bytes - out.available_bytes);
    }
    if (out.available_bytes > out.total_bytes) return out;
    out.used_bytes = out.total_bytes - out.available_bytes;
    out.valid = true;
    if (values.count("SwapTotal:") && values.count("SwapFree:") && values["SwapFree:"] <= values["SwapTotal:"]) {
        out.swap_total_bytes = values["SwapTotal:"];
        out.swap_used_bytes = out.swap_total_bytes - values["SwapFree:"];
        out.swap_valid = true;
    }
    return out;
}
void Disks(const std::string& path, HostSample& out) {
    out.disks_sampled = true;
    std::ifstream in(path);
    out.disks_valid = bool(in);
    std::string device, mount, fs, rest;
    std::set<std::string> seen;
    while (in >> device >> mount >> fs) {
        std::getline(in, rest);
        if (mount != "/" && mount != "/home" && mount != "/var" && mount != "/data") continue;
        // Remote mounts must never block base telemetry.
        if (fs != "ext4" && fs != "xfs" && fs != "btrfs" && fs != "f2fs" && fs != "overlay") continue;
        if (!seen.insert(mount).second) continue;
        struct statvfs st {};
        if (statvfs(mount.c_str(), &st)) { out.disks_valid = false; continue; }
        DiskSample d;
        d.mount_point = mount; d.filesystem = fs;
        d.total_bytes = st.f_frsize * std::uint64_t(st.f_blocks);
        d.available_bytes = st.f_frsize * std::uint64_t(st.f_bavail);
        d.used_bytes = st.f_frsize * std::uint64_t(st.f_blocks - st.f_bfree);
        out.disks.push_back(d);
    }
}
} // namespace
HostSample HostSampler::Sample() {
    HostSample out;
    out.monotonic_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    const auto wall = std::chrono::system_clock::now();
    out.unix_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(wall.time_since_epoch()).count();
    std::time_t stamp = std::chrono::system_clock::to_time_t(wall);
    std::tm local{};
    if (localtime_r(&stamp, &local)) {
        char buffer[128];
        if (strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S %z", &local)) out.local_time = buffer;
        out.timezone = Line(options_.etc_root + "/timezone");
        if (out.timezone.empty() && strftime(buffer, sizeof(buffer), "%Z", &local)) out.timezone = buffer;
    }
    { std::ifstream in(options_.proc_root + "/uptime");
      out.uptime_valid = bool(in >> out.uptime_sec) && std::isfinite(out.uptime_sec) && out.uptime_sec >= 0; }
    { std::ifstream in(options_.proc_root + "/loadavg");
      out.load_valid = bool(in >> out.load1 >> out.load5 >> out.load15) &&
          std::isfinite(out.load1) && std::isfinite(out.load5) && std::isfinite(out.load15) &&
          out.load1 >= 0 && out.load5 >= 0 && out.load15 >= 0; }
    out.memory = Memory(options_.proc_root + "/meminfo");
    out.boot_id = Line(options_.proc_root + "/sys/kernel/random/boot_id");
    struct utsname name {};
    if (uname(&name) == 0) {
        out.info.hostname = name.nodename; out.info.architecture = name.machine;
        out.info.kernel_release = name.release; out.info.kernel_valid = true;
    }
    const auto os = OsRelease(options_.etc_root + "/os-release");
    auto prop = [&](const char* key) { auto it = os.find(key); return it == os.end() ? std::string() : it->second; };
    out.info.os_name = prop("PRETTY_NAME");
    if (out.info.os_name.empty()) out.info.os_name = prop("NAME");
    out.info.os_version = prop("VERSION_ID");
    out.info.os_valid = !out.info.os_name.empty();
    bool online = false, isolated = false;
    out.info.online_cpus = Line(options_.sys_root + "/devices/system/cpu/online", &online);
    out.info.isolated_cpus = Line(options_.sys_root + "/devices/system/cpu/isolated", &isolated);
    out.info.cpu_lists_valid = online && isolated;
    if (options_.sample_disks) Disks(options_.proc_root + "/mounts", out);

    std::ifstream stat(options_.proc_root + "/stat");
    std::map<int, Counters> current;
    std::string line;
    bool malformed = !stat;
    while (std::getline(stat, line)) {
        std::istringstream row(line);
        std::string label; row >> label;
        if (label.rfind("cpu", 0) != 0) continue;
        int id = -1;
        if (label != "cpu") {
            const auto suffix = label.substr(3);
            if (suffix.empty() || suffix.size() > 5 || suffix.find_first_not_of("0123456789") != std::string::npos) { malformed = true; continue; }
            id = std::stoi(suffix);
        }
        std::uint64_t v[8]{};
        bool valid = true;
        for (auto& n : v) if (!(row >> n)) { valid = false; break; }
        Counters c;
        for (const auto n : v) {
            if (n > UINT64_MAX - c.total) { valid = false; break; }
            c.total += n;
        }
        if (!valid || current.count(id)) { malformed = true; continue; }
        c.idle = v[3] + v[4];
        current[id] = c;
    }
    // Topology changes require a new baseline; no matching by vector index.
    const bool topology = !malformed && current.count(-1) && current.size() > 1 &&
        current.size() == previous_.size() && std::equal(current.begin(), current.end(), previous_.begin(),
            [](const auto& a, const auto& b) { return a.first == b.first; });
    out.cpu.online_cpus = current.size() - current.count(-1);
    if (topology && previous_ns_ && out.monotonic_ns > previous_ns_)
        out.cpu.window_ms = std::min<std::uint64_t>((out.monotonic_ns - previous_ns_) / 1000000, UINT32_MAX);
    for (const auto& [id, c] : current) {
        double ratio = 0;
        bool valid = false;
        if (topology) {
            const auto& p = previous_.at(id);
            if (c.total > p.total && c.idle >= p.idle && c.idle - p.idle <= c.total - p.total) {
                ratio = 1.0 - double(c.idle - p.idle) / double(c.total - p.total);
                valid = true;
            }
        }
        if (id < 0) { out.cpu.usage_ratio = ratio; out.cpu.valid = valid; }
        else {
            out.cpu.core_ids.push_back(id); out.cpu.core_usage_ratio.push_back(ratio);
            out.cpu.core_valid.push_back(valid);
        }
    }
    previous_ = malformed ? std::map<int, Counters>{} : std::move(current);
    previous_ns_ = out.monotonic_ns;
    return out;
}
} // namespace openember::host_monitor
