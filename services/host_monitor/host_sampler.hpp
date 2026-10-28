/*
 * Copyright (c) 2022-2026, OpenEmber Team
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace openember::host_monitor {

struct CpuSample {
    double usage_ratio = 0.0;
    std::uint32_t online_cpus = 0;
    std::vector<double> core_usage_ratio;
};

struct MemorySample {
    std::uint64_t total_bytes = 0;
    std::uint64_t available_bytes = 0;
    std::uint64_t used_bytes = 0;
    std::uint64_t swap_total_bytes = 0;
    std::uint64_t swap_used_bytes = 0;
};

struct DiskSample {
    std::string mount_point;
    std::string filesystem;
    std::uint64_t total_bytes = 0;
    std::uint64_t used_bytes = 0;
    std::uint64_t available_bytes = 0;
};

struct HostSample {
    double uptime_sec = 0.0;
    double load1 = 0.0;
    double load5 = 0.0;
    double load15 = 0.0;
    CpuSample cpu;
    MemorySample memory;
    std::vector<DiskSample> disks;
};

// Stateful sampler: CPU ratios need a previous /proc/stat reading.
class HostSampler {
public:
    HostSample Sample();

private:
    struct CpuCounters {
        std::uint64_t idle = 0;
        std::uint64_t total = 0;
    };

    bool have_prev_cpu_ = false;
    CpuCounters prev_total_{};
    std::vector<CpuCounters> prev_cores_;
};

}  // namespace openember::host_monitor
