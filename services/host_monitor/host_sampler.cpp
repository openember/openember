/*
 * Copyright (c) 2022-2026, OpenEmber Team
 * SPDX-License-Identifier: Apache-2.0
 */

#include "host_sampler.hpp"

#include <cctype>
#include <fstream>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include <sys/statvfs.h>

namespace openember::host_monitor {
namespace {

struct RawCpu {
    std::uint64_t idle = 0;
    std::uint64_t total = 0;
};

bool ReadLoadAvg(double* load1, double* load5, double* load15) {
    std::ifstream in("/proc/loadavg");
    if (!in) {
        return false;
    }
    in >> *load1 >> *load5 >> *load15;
    return static_cast<bool>(in);
}

bool ReadUptime(double* uptime_sec) {
    std::ifstream in("/proc/uptime");
    if (!in) {
        return false;
    }
    in >> *uptime_sec;
    return static_cast<bool>(in);
}

bool ReadMemInfo(MemorySample* out) {
    std::ifstream in("/proc/meminfo");
    if (!in) {
        return false;
    }

    std::uint64_t mem_total_kb = 0;
    std::uint64_t mem_available_kb = 0;
    std::uint64_t mem_free_kb = 0;
    std::uint64_t buffers_kb = 0;
    std::uint64_t cached_kb = 0;
    std::uint64_t swap_total_kb = 0;
    std::uint64_t swap_free_kb = 0;

    std::string key;
    std::uint64_t value = 0;
    std::string unit;
    while (in >> key >> value >> unit) {
        if (key == "MemTotal:") {
            mem_total_kb = value;
        } else if (key == "MemAvailable:") {
            mem_available_kb = value;
        } else if (key == "MemFree:") {
            mem_free_kb = value;
        } else if (key == "Buffers:") {
            buffers_kb = value;
        } else if (key == "Cached:") {
            cached_kb = value;
        } else if (key == "SwapTotal:") {
            swap_total_kb = value;
        } else if (key == "SwapFree:") {
            swap_free_kb = value;
        }
    }

    if (mem_available_kb == 0) {
        mem_available_kb = mem_free_kb + buffers_kb + cached_kb;
    }

    out->total_bytes = mem_total_kb * 1024ULL;
    out->available_bytes = mem_available_kb * 1024ULL;
    if (out->total_bytes >= out->available_bytes) {
        out->used_bytes = out->total_bytes - out->available_bytes;
    }
    out->swap_total_bytes = swap_total_kb * 1024ULL;
    if (swap_total_kb >= swap_free_kb) {
        out->swap_used_bytes = (swap_total_kb - swap_free_kb) * 1024ULL;
    }
    return out->total_bytes > 0;
}

bool ParseCpuCounters(const std::string& line, RawCpu* out) {
    std::istringstream iss(line);
    std::string label;
    iss >> label;
    if (label.rfind("cpu", 0) != 0) {
        return false;
    }

    std::uint64_t user = 0;
    std::uint64_t nice = 0;
    std::uint64_t system = 0;
    std::uint64_t idle = 0;
    std::uint64_t iowait = 0;
    std::uint64_t irq = 0;
    std::uint64_t softirq = 0;
    std::uint64_t steal = 0;
    iss >> user >> nice >> system >> idle >> iowait >> irq >> softirq >> steal;

    const std::uint64_t idle_all = idle + iowait;
    const std::uint64_t non_idle = user + nice + system + irq + softirq + steal;
    out->idle = idle_all;
    out->total = idle_all + non_idle;
    return out->total > 0;
}

double UsageRatio(const RawCpu& prev, const RawCpu& cur) {
    if (cur.total <= prev.total) {
        return 0.0;
    }
    const double total_d = static_cast<double>(cur.total - prev.total);
    const double idle_d = static_cast<double>(cur.idle - prev.idle);
    double usage = (total_d - idle_d) / total_d;
    if (usage < 0.0) {
        usage = 0.0;
    }
    if (usage > 1.0) {
        usage = 1.0;
    }
    return usage;
}

bool IsInterestingFs(const std::string& fs) {
    static const std::set<std::string> kSkip = {
        "proc",       "sysfs",     "devtmpfs", "devpts",   "tmpfs",
        "cgroup",     "cgroup2",   "pstore",   "bpf",      "debugfs",
        "tracefs",    "securityfs","hugetlbfs","mqueue",   "fusectl",
        "configfs",   "rpc_pipefs","binfmt_misc","autofs", "overlay",
    };
    return kSkip.find(fs) == kSkip.end();
}

std::vector<DiskSample> SampleDisks() {
    std::vector<DiskSample> disks;
    std::ifstream mounts("/proc/mounts");
    if (!mounts) {
        return disks;
    }

    std::set<std::string> seen;
    std::string device;
    std::string mount;
    std::string fs;
    std::string rest;
    while (mounts >> device >> mount >> fs) {
        std::getline(mounts, rest);
        if (!IsInterestingFs(fs)) {
            continue;
        }
        if (mount != "/" && mount != "/home" && mount != "/var" && mount != "/data" &&
            mount != "/tmp") {
            continue;
        }
        if (!seen.insert(mount).second) {
            continue;
        }

        struct statvfs st {};
        if (statvfs(mount.c_str(), &st) != 0) {
            continue;
        }
        const std::uint64_t block = static_cast<std::uint64_t>(st.f_frsize);
        DiskSample d;
        d.mount_point = mount;
        d.filesystem = fs;
        d.total_bytes = block * static_cast<std::uint64_t>(st.f_blocks);
        d.available_bytes = block * static_cast<std::uint64_t>(st.f_bavail);
        if (d.total_bytes >= d.available_bytes) {
            d.used_bytes = d.total_bytes - d.available_bytes;
        }
        disks.push_back(std::move(d));
    }
    return disks;
}

}  // namespace

HostSample HostSampler::Sample() {
    HostSample sample;
    (void)ReadUptime(&sample.uptime_sec);
    (void)ReadLoadAvg(&sample.load1, &sample.load5, &sample.load15);
    (void)ReadMemInfo(&sample.memory);
    sample.disks = SampleDisks();

    std::ifstream stat("/proc/stat");
    if (!stat) {
        return sample;
    }

    std::vector<RawCpu> current;
    std::string line;
    while (std::getline(stat, line)) {
        if (line.rfind("cpu", 0) != 0) {
            if (!current.empty()) {
                break;
            }
            continue;
        }
        RawCpu counters;
        if (!ParseCpuCounters(line, &counters)) {
            continue;
        }
        current.push_back(counters);
    }

    if (current.empty()) {
        return sample;
    }

    sample.cpu.online_cpus =
        current.size() > 1 ? static_cast<std::uint32_t>(current.size() - 1) : 1U;

    if (!have_prev_cpu_ || prev_cores_.size() + 1 != current.size()) {
        prev_total_ = {current[0].idle, current[0].total};
        prev_cores_.clear();
        for (std::size_t i = 1; i < current.size(); ++i) {
            prev_cores_.push_back({current[i].idle, current[i].total});
        }
        have_prev_cpu_ = true;
        return sample;
    }

    const RawCpu prev_agg{prev_total_.idle, prev_total_.total};
    sample.cpu.usage_ratio = UsageRatio(prev_agg, current[0]);
    sample.cpu.core_usage_ratio.clear();
    for (std::size_t i = 1; i < current.size(); ++i) {
        const RawCpu prev{prev_cores_[i - 1].idle, prev_cores_[i - 1].total};
        sample.cpu.core_usage_ratio.push_back(UsageRatio(prev, current[i]));
    }

    prev_total_ = {current[0].idle, current[0].total};
    prev_cores_.clear();
    for (std::size_t i = 1; i < current.size(); ++i) {
        prev_cores_.push_back({current[i].idle, current[i].total});
    }
    return sample;
}

}  // namespace openember::host_monitor
