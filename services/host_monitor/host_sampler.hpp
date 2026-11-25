/*
 * Copyright (c) 2022-2026, OpenEmber Team
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once
#include <cstdint>
#include <map>
#include <string>
#include <vector>
#include <utility>

namespace openember::host_monitor {
struct CpuSample {
    double usage_ratio = 0;
    bool valid = false;
    std::uint32_t online_cpus = 0, window_ms = 0;
    std::vector<std::uint32_t> core_ids;
    std::vector<double> core_usage_ratio;
    std::vector<bool> core_valid;
};
struct MemorySample {
    std::uint64_t total_bytes = 0, available_bytes = 0, used_bytes = 0;
    std::uint64_t swap_total_bytes = 0, swap_used_bytes = 0;
    bool valid = false, available_estimated = false, swap_valid = false;
};
struct DiskSample {
    std::string mount_point, filesystem;
    std::uint64_t total_bytes = 0, used_bytes = 0, available_bytes = 0;
};
struct HostInfo {
    std::string hostname, architecture, kernel_release, os_name, os_version;
    std::string online_cpus, isolated_cpus;
    bool kernel_valid = false, os_valid = false, cpu_lists_valid = false;
};
struct HostSample {
    double uptime_sec = 0, load1 = 0, load5 = 0, load15 = 0;
    bool uptime_valid = false, load_valid = false;
    std::uint64_t monotonic_ns = 0, unix_ns = 0;
    std::string boot_id, timezone, local_time;
    CpuSample cpu;
    MemorySample memory;
    HostInfo info;
    std::vector<DiskSample> disks;
    bool disks_sampled = false, disks_valid = false;
};
struct SamplerOptions {
    std::string proc_root = "/proc", sys_root = "/sys", etc_root = "/etc";
    bool sample_disks = false;
};
// Linux read-only sampler; alternate roots permit deterministic fixture tests.
class HostSampler {
public:
    explicit HostSampler(SamplerOptions options = {}) : options_(std::move(options)) {}
    HostSample Sample();
private:
    struct Counters { std::uint64_t idle = 0, total = 0; };
    SamplerOptions options_;
    std::map<int, Counters> previous_;
    std::uint64_t previous_ns_ = 0;
};
}  // namespace openember::host_monitor
