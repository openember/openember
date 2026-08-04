#include "process_manager.hpp"

#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cerrno>
#include <chrono>
#include <cstring>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#define LOG_TAG "launch_manager"
#include "openember.h"

namespace openember::launch {
namespace {

std::vector<char*> BuildArgv(const ProcessSpec& spec) {
    std::vector<char*> argv;
    argv.reserve(spec.args.size() + 2);
    argv.push_back(const_cast<char*>(spec.command.c_str()));
    for (const auto& arg : spec.args) {
        argv.push_back(const_cast<char*>(arg.c_str()));
    }
    argv.push_back(nullptr);
    return argv;
}

bool ExitedSuccessfully(int status) {
    return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

std::uint64_t UnixTimeNs() {
    const auto now = std::chrono::system_clock::now().time_since_epoch();
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(now).count());
}

int ExitCodeFromStatus(int status) {
    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    }
    if (WIFSIGNALED(status)) {
        return -WTERMSIG(status);
    }
    return status;
}

std::string StatusText(int status) {
    if (WIFEXITED(status)) {
        return "exit_code=" + std::to_string(WEXITSTATUS(status));
    }
    if (WIFSIGNALED(status)) {
        return "signal=" + std::to_string(WTERMSIG(status));
    }
    return "status=" + std::to_string(status);
}

}  // namespace

ProcessManager::ProcessManager(std::vector<ProcessSpec> specs) {
    processes_.reserve(specs.size());
    for (auto& spec : specs) {
        processes_.push_back(ManagedProcess{std::move(spec)});
    }
}

void ProcessManager::SetEventCallback(EventCallback callback) {
    event_callback_ = std::move(callback);
}

bool ProcessManager::StartAll() {
    bool ok = true;
    for (auto& process : processes_) {
        ok = StartOne(process) && ok;
    }
    return ok;
}

bool ProcessManager::StartOne(ManagedProcess& process) {
    if (process.pid > 0) {
        return true;
    }

    LOG_I("Start process: name=%s command=%s restart=%s",
          process.spec.name.c_str(),
          process.spec.command.c_str(),
          RestartPolicyName(process.spec.restart));
    process.state = ProcessState::kStarting;
    process.start_time_unix_ns = UnixTimeNs();
    process.stop_time_unix_ns = 0;
    process.has_heartbeat = false;
    EmitEvent(process, process.state, 0, 0, "starting");

    const pid_t pid = fork();
    if (pid < 0) {
        LOG_E("fork failed for %s: %s", process.spec.name.c_str(), strerror(errno));
        process.state = ProcessState::kFailed;
        EmitEvent(process, process.state, errno, UnixTimeNs(), strerror(errno));
        return false;
    }

    if (pid == 0) {
        if (!process.spec.working_directory.empty() &&
            chdir(process.spec.working_directory.c_str()) != 0) {
            perror("chdir");
            _exit(127);
        }

        for (const auto& [key, value] : process.spec.env) {
            setenv(key.c_str(), value.c_str(), 1);
        }

        auto argv = BuildArgv(process.spec);
        if (process.spec.command.find('/') != std::string::npos) {
            execv(process.spec.command.c_str(), argv.data());
        } else {
            execvp(process.spec.command.c_str(), argv.data());
        }
        perror("exec");
        _exit(127);
    }

    process.pid = pid;
    process.state = ProcessState::kRunning;
    process.start_time = std::chrono::steady_clock::now();
    LOG_I("Started process: name=%s pid=%d", process.spec.name.c_str(), static_cast<int>(pid));
    EmitEvent(process, process.state, 0, 0, "running");
    return true;
}

void ProcessManager::Poll() {
    int status = 0;
    while (true) {
        const pid_t pid = waitpid(-1, &status, WNOHANG);
        if (pid > 0) {
            HandleExit(pid, status);
            continue;
        }
        if (pid == 0) {
            CheckHeartbeatTimeouts();
            return;
        }
        if (errno == EINTR) {
            continue;
        }
        if (errno != ECHILD) {
            LOG_E("waitpid failed: %s", strerror(errno));
        }
        CheckHeartbeatTimeouts();
        return;
    }
}

void ProcessManager::HandleExit(pid_t pid, int status) {
    ManagedProcess* process = FindByPid(pid);
    if (!process) {
        LOG_I("Unmanaged child exited: pid=%d %s", static_cast<int>(pid), StatusText(status).c_str());
        return;
    }

    LOG_I("Process exited: name=%s pid=%d %s",
          process->spec.name.c_str(),
          static_cast<int>(pid),
          StatusText(status).c_str());

    const auto exit_code = ExitCodeFromStatus(status);
    const auto stop_time_unix_ns = UnixTimeNs();
    const auto state = stopping_
        ? ProcessState::kStopped
        : (ExitedSuccessfully(status) ? ProcessState::kExited : ProcessState::kFailed);

    process->state = state;
    process->stop_time_unix_ns = stop_time_unix_ns;
    EmitEvent(*process, state, exit_code, stop_time_unix_ns, StatusText(status));
    process->pid = -1;

    if (!stopping_ && ShouldRestart(*process, status)) {
        ++process->restart_count;
        LOG_I("Restart process: name=%s restart_count=%u",
              process->spec.name.c_str(),
              process->restart_count);
        (void)StartOne(*process);
    }
}

bool ProcessManager::ShouldRestart(const ManagedProcess& process, int status) const {
    if (process.spec.restart == RestartPolicy::kNever) {
        return false;
    }
    if (process.spec.restart_limit != 0 &&
        process.restart_count >= process.spec.restart_limit) {
        LOG_E("Restart limit reached: name=%s limit=%u",
              process.spec.name.c_str(),
              process.spec.restart_limit);
        return false;
    }
    if (process.spec.restart == RestartPolicy::kAlways) {
        return true;
    }
    return !ExitedSuccessfully(status);
}

void ProcessManager::StopAll() {
    stopping_ = true;
    for (auto& process : processes_) {
        StopOne(process);
    }

    bool any_running = true;
    while (any_running) {
        Poll();
        any_running = false;
        const auto now = std::chrono::steady_clock::now();

        for (auto& process : processes_) {
            if (process.pid <= 0) {
                continue;
            }
            any_running = true;
            if (now >= process.stop_deadline) {
                LOG_E("Process stop timeout, kill: name=%s pid=%d",
                      process.spec.name.c_str(),
                      static_cast<int>(process.pid));
                (void)kill(process.pid, SIGKILL);
                process.stop_deadline = now + std::chrono::seconds(1);
            }
        }

        if (any_running) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
}

bool ProcessManager::MarkHeartbeat(const std::string& process_name) {
    for (auto& process : processes_) {
        if (process.spec.name != process_name) {
            continue;
        }
        const auto now = std::chrono::steady_clock::now();
        if (!process.has_heartbeat) {
            LOG_I("Process heartbeat observed: name=%s", process.spec.name.c_str());
        }
        process.has_heartbeat = true;
        process.last_heartbeat_time = now;
        return true;
    }
    return false;
}

void ProcessManager::StopOne(ManagedProcess& process, const std::string& message) {
    if (process.pid <= 0) {
        return;
    }

    LOG_I("Stop process: name=%s pid=%d",
          process.spec.name.c_str(),
          static_cast<int>(process.pid));
    process.state = ProcessState::kStopping;
    process.stop_deadline =
        std::chrono::steady_clock::now() + process.spec.shutdown_timeout;
    EmitEvent(process, process.state, 0, 0, message);
    if (kill(process.pid, SIGTERM) != 0 && errno != ESRCH) {
        LOG_E("SIGTERM failed for %s: %s", process.spec.name.c_str(), strerror(errno));
    }
}

void ProcessManager::CheckHeartbeatTimeouts() {
    if (stopping_) {
        return;
    }

    const auto now = std::chrono::steady_clock::now();
    for (auto& process : processes_) {
        if (process.pid <= 0 || process.state == ProcessState::kStopping) {
            continue;
        }

        if (!process.has_heartbeat &&
            process.spec.startup_timeout.count() > 0 &&
            now - process.start_time > process.spec.startup_timeout) {
            LOG_E("Startup heartbeat timeout: name=%s pid=%d",
                  process.spec.name.c_str(),
                  static_cast<int>(process.pid));
            StopOne(process, "startup heartbeat timeout");
            continue;
        }

        if (process.has_heartbeat &&
            process.spec.heartbeat_timeout.count() > 0 &&
            now - process.last_heartbeat_time > process.spec.heartbeat_timeout) {
            LOG_E("Heartbeat timeout: name=%s pid=%d",
                  process.spec.name.c_str(),
                  static_cast<int>(process.pid));
            StopOne(process, "heartbeat timeout");
        }
    }
}

ProcessManager::ManagedProcess* ProcessManager::FindByPid(pid_t pid) {
    for (auto& process : processes_) {
        if (process.pid == pid) {
            return &process;
        }
    }
    return nullptr;
}

void ProcessManager::EmitEvent(const ManagedProcess& process,
                               ProcessState state,
                               int exit_code,
                               std::uint64_t stop_time_unix_ns,
                               const std::string& message) const {
    if (!event_callback_) {
        return;
    }

    ProcessEvent event;
    event.spec = process.spec;
    event.pid = process.pid;
    event.state = state;
    event.exit_code = exit_code;
    event.start_time_unix_ns = process.start_time_unix_ns;
    event.stop_time_unix_ns = stop_time_unix_ns;
    event.message = message;
    event_callback_(event);
}

}  // namespace openember::launch
