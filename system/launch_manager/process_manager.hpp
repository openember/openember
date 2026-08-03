#pragma once

#include <sys/types.h>

#include <chrono>
#include <cstdint>
#include <vector>

#include "launch_config.hpp"

namespace openember::launch {

class ProcessManager {
public:
    explicit ProcessManager(std::vector<ProcessSpec> specs);

    bool StartAll();
    void Poll();
    void StopAll();

private:
    enum class State {
        kStopped,
        kStarting,
        kRunning,
        kStopping,
        kExited,
        kFailed,
    };

    struct ManagedProcess {
        ProcessSpec spec;
        pid_t pid = -1;
        State state = State::kStopped;
        std::uint32_t restart_count = 0;
        std::chrono::steady_clock::time_point start_time;
        std::chrono::steady_clock::time_point stop_deadline;
    };

    bool StartOne(ManagedProcess& process);
    void HandleExit(pid_t pid, int status);
    bool ShouldRestart(const ManagedProcess& process, int status) const;
    void StopOne(ManagedProcess& process);
    ManagedProcess* FindByPid(pid_t pid);

private:
    std::vector<ManagedProcess> processes_;
    bool stopping_ = false;
};

}  // namespace openember::launch
