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

    ~ProcessManager();
    void SetEventCallback(EventCallback callback);
    bool StartAll();
    ProcessEvent StartProcess(ProcessSpec spec);
    ProcessEvent StopProcess(const std::string& name,
                             pid_t pid,
                             std::chrono::milliseconds timeout);
    void Poll();
    void StopAll();
    bool MarkHeartbeat(const std::string& process_name);

private:
    struct ManagedProcess {
        ProcessSpec spec;
        pid_t pid = -1;
        ProcessState state = ProcessState::kStopped;
        std::uint32_t restart_count = 0;
        std::chrono::steady_clock::time_point start_time{};
        std::uint64_t start_time_unix_ns = 0;
        std::uint64_t stop_time_unix_ns = 0;
        int ready_fd = -1;
        bool ready = false;
        bool desired = false;
        std::chrono::steady_clock::time_point retry_at{};
        bool has_heartbeat = false;
        bool intentional_stop = false;
        bool supervision_failure = false;
        std::chrono::steady_clock::time_point last_heartbeat_time{};
        std::chrono::steady_clock::time_point stop_deadline{};
    };

    bool DependenciesReady(const ManagedProcess& process);
    bool DependsOn(const ManagedProcess& process, const std::string& name);
    void StopDependents(const std::string& name);
    void CheckReadiness();
    void Schedule();
    void CheckStopDeadlines();
    bool StartOne(ManagedProcess& process);
    ManagedProcess* FindByName(const std::string& name);
    void HandleExit(pid_t pid, int status);
    bool ShouldRestart(const ManagedProcess& process, int status) const;
    void StopOne(ManagedProcess& process,
                 const std::string& message = "stopping",
                 std::chrono::milliseconds timeout = std::chrono::milliseconds{0},
                 bool intentional_stop = true);
    void CheckHeartbeatTimeouts();
    ManagedProcess* FindByPid(pid_t pid);
    ProcessEvent MakeEvent(const ManagedProcess& process,
                           ProcessState state,
                           int exit_code,
                           std::uint64_t stop_time_unix_ns,
                           const std::string& message) const;
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
