/* SPDX-License-Identifier: Apache-2.0 */
#include "host_sampler.hpp"
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <unistd.h>
namespace fs = std::filesystem;
void Check(bool ok) { if (!ok) throw std::runtime_error("host sampler assertion failed"); }
int main() {
    const auto root = fs::temp_directory_path() / ("ember-host-" + std::to_string(getpid()));
    fs::create_directories(root / "proc/sys/kernel/random");
    fs::create_directories(root / "sys/devices/system/cpu");
    fs::create_directories(root / "etc");
    const auto write = [&](const char* name, const char* text) { std::ofstream(root / name) << text; };
    try {
        write("proc/uptime", "1234.5 42\n"); write("proc/loadavg", "0.1 0.2 0.3 1/100 9\n");
        write("proc/sys/kernel/random/boot_id", "test-boot\n");
        write("sys/devices/system/cpu/online", "0,4\n");
        write("sys/devices/system/cpu/isolated", "\n");
        write("etc/os-release", "NAME=Demo\nPRETTY_NAME=\"Demo OS\"\nVERSION_ID=\"1\"\n");
        write("proc/meminfo", "MemTotal: 1024 kB\nHugePages_Total: 0\nMemAvailable: 0 kB\nMemFree: 128 kB\nBuffers: 128 kB\nCached: 128 kB\nSwapTotal: 0 kB\nSwapFree: 0 kB\n");
        write("proc/stat", "cpu 100 0 0 100 0 0 0 0\ncpu0 50 0 0 50 0 0 0 0\ncpu4 50 0 0 50 0 0 0 0\n");
        openember::host_monitor::HostSampler sampler({(root/"proc").string(),(root/"sys").string(),(root/"etc").string()});
        auto first = sampler.Sample();
        Check(!first.cpu.valid && first.cpu.online_cpus == 2);
        Check(first.memory.valid && first.memory.available_bytes == 0 && !first.memory.available_estimated);
        Check(first.memory.used_bytes == first.memory.total_bytes && first.memory.swap_valid);
        Check(first.uptime_valid && first.uptime_sec == 1234.5 && first.load_valid);
        Check(first.boot_id == "test-boot" && first.info.os_name == "Demo OS");
        Check(first.info.cpu_lists_valid && first.info.isolated_cpus.empty());
        Check(!first.disks_sampled);
        write("proc/stat", "cpu 150 0 0 150 0 0 0 0\ncpu0 60 0 0 90 0 0 0 0\ncpu4 90 0 0 60 0 0 0 0\n");
        auto second = sampler.Sample();
        Check(second.cpu.valid && std::abs(second.cpu.usage_ratio - .5) < 1e-9);
        Check(second.cpu.core_ids == std::vector<std::uint32_t>({0,4}));
        Check(std::abs(second.cpu.core_usage_ratio[0] - .2) < 1e-9 && std::abs(second.cpu.core_usage_ratio[1] - .8) < 1e-9);
        Check(!sampler.Sample().cpu.valid); // Frozen counters are not 0% use.
        write("proc/stat", "cpu 1 0 0 1 0 0 0 0\ncpu0 1 0 0 1 0 0 0 0\ncpu4 1 0 0 1 0 0 0 0\n");
        Check(!sampler.Sample().cpu.valid); // Counter reset.
        write("proc/stat", "cpu 50 0 0 50 0 0 0 0\ncpu0 25 0 0 25 0 0 0 0\ncpu3 25 0 0 25 0 0 0 0\n");
        Check(!sampler.Sample().cpu.valid); // Same core count, different topology.
        write("proc/meminfo", "MemTotal: 1024 kB\nHugePages_Free: 0\nMemFree: 128 kB\nBuffers: 128 kB\nCached: 128 kB\n");
        auto estimated = sampler.Sample();
        Check(estimated.memory.valid && estimated.memory.available_estimated && estimated.memory.available_bytes == 384 * 1024);
        fs::remove(root / "proc/meminfo"); fs::remove(root / "proc/uptime"); fs::remove(root / "proc/stat");
        auto missing = sampler.Sample();
        Check(!missing.memory.valid && !missing.uptime_valid && !missing.cpu.valid && missing.load_valid);
        Check(missing.unix_ns > 0 && missing.monotonic_ns > 0 && !missing.local_time.empty());
        fs::remove_all(root);
        std::cout << "HOST_SAMPLER_OK sparse CPU IDs, deltas, reset/hotplug, zero/fallback memory, partial failure\n";
    } catch (...) { fs::remove_all(root); throw; }
}
