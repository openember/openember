#pragma once

#include <sys/types.h>

#include <chrono>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "launch_config.hpp"

namespace openember::launch {

class ProcessManager {
public:
    enum class ProcessState {
        kStarting,
        kRunning,
        kStopping,
        kStopped,
        kExited,
        kFailed,
    };

    struct ProcessEvent {
        ProcessSpec spec;
        pid_t pid = -1;
        ProcessState state = ProcessState::kStopped;
        int exit_code = 0;
        std::uint64_t start_time_unix_ns = 0;
        std::uint64_t stop_time_unix_ns = 0;
        std::string message;
    };

    using EventCallback = std::function<void(const ProcessEvent&)>;

    explicit ProcessManager(std::vector<ProcessSpec> specs);

    void SetEventCallback(EventCallback callback);
    bool StartAll();
    void Poll();
    void StopAll();
    bool MarkHeartbeat(const std::string& process_name);

private:
    struct ManagedProcess {
        ProcessSpec spec;
        pid_t pid = -1;
        ProcessState state = ProcessState::kStopped;
        std::uint32_t restart_count = 0;
        std::chrono::steady_clock::time_point start_time;
        std::uint64_t start_time_unix_ns = 0;
        std::uint64_t stop_time_unix_ns = 0;
        bool has_heartbeat = false;
        std::chrono::steady_clock::time_point last_heartbeat_time;
        std::chrono::steady_clock::time_point stop_deadline;
    };

    bool StartOne(ManagedProcess& process);
    void HandleExit(pid_t pid, int status);
    bool ShouldRestart(const ManagedProcess& process, int status) const;
    void StopOne(ManagedProcess& process, const std::string& message = "stopping");
    void CheckHeartbeatTimeouts();
    ManagedProcess* FindByPid(pid_t pid);
    void EmitEvent(const ManagedProcess& process,
                   ProcessState state,
                   int exit_code,
                   std::uint64_t stop_time_unix_ns,
                   const std::string& message) const;

private:
    std::vector<ManagedProcess> processes_;
    EventCallback event_callback_;
    bool stopping_ = false;
};

}  // namespace openember::launch
