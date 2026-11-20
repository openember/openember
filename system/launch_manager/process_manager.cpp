#include "process_manager.hpp"

#include <signal.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <algorithm>
#include <stdexcept>
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
    ValidateLaunchGraph(specs);
    processes_.reserve(specs.size());
    for (auto& spec : specs) {
        processes_.push_back(ManagedProcess{std::move(spec)});
    }
}

void ProcessManager::SetEventCallback(EventCallback callback) {
    event_callback_ = std::move(callback);
}

ProcessManager::~ProcessManager() {
    StopAll();
    for (auto& process : processes_)
        if (process.ready_fd >= 0) close(process.ready_fd);
}
bool ProcessManager::StartAll() {
    for (auto& process : processes_) process.desired = true;
    Schedule();
    return true;
}
bool ProcessManager::DependenciesReady(const ManagedProcess& process) {
    for (const auto& name : process.spec.depends_on) {
        const auto* dependency = FindByName(name);
        if (!dependency || !dependency->ready || dependency->state != ProcessState::kRunning)
            return false;
    }
    return true;
}
bool ProcessManager::DependsOn(const ManagedProcess& process, const std::string& name) {
    for (const auto& dependency : process.spec.depends_on) {
        if (dependency == name) return true;
        const auto* parent = FindByName(dependency);
        if (parent && DependsOn(*parent, name)) return true;
    }
    return false;
}
void ProcessManager::StopDependents(const std::string& name) {
    for (auto& dependent : processes_) {
        if (dependent.pid > 0 && dependent.state != ProcessState::kStopping &&
            DependsOn(dependent, name)) {
            StopDependents(dependent.spec.name);
            StopOne(dependent, "dependency unavailable", {}, true);
        }
    }
}
void ProcessManager::Schedule() {
    if (stopping_) return;
    // Revisit the graph so file order need not be topological.
    for (std::size_t pass = 0; pass < processes_.size(); ++pass)
        for (auto& process : processes_)
            if (process.desired && process.pid <= 0 && DependenciesReady(process) &&
                std::chrono::steady_clock::now() >= process.retry_at) {
                if (!StartOne(process)) process.desired = false;
            }
}
void ProcessManager::CheckReadiness() {
    for (auto& process : processes_) {
        if (process.ready_fd < 0 || process.state != ProcessState::kStarting) continue;
        char value = 0;
        const auto count = recv(process.ready_fd, &value, 1, MSG_DONTWAIT);
        if (count == 1 && value == 'R') {
            close(process.ready_fd); process.ready_fd = -1;
            process.ready = true; process.state = ProcessState::kRunning;
            EmitEvent(process, process.state, 0, 0, "ready notification received");
        } else if (count == 0 || count > 0 ||
                   (count < 0 && errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR) ||
                   std::chrono::steady_clock::now() - process.start_time > process.spec.startup_timeout) {
            StopOne(process, "readiness failed or timed out", {}, false);
        }
    }
}
void ProcessManager::CheckStopDeadlines() {
    for (auto& process : processes_)
        if (process.pid > 0 && process.state == ProcessState::kStopping &&
            std::chrono::steady_clock::now() >= process.stop_deadline) {
            (void)kill(process.pid, SIGKILL);
            process.stop_deadline = std::chrono::steady_clock::now() + std::chrono::seconds(1);
        }
}

ProcessManager::ProcessEvent ProcessManager::StartProcess(ProcessSpec spec) {
    if (spec.name.empty()) {
        spec.name = spec.command;
    }

    if (spec.name.empty() || spec.command.empty()) {
        ManagedProcess invalid;
        invalid.spec = std::move(spec);
        invalid.state = ProcessState::kFailed;
        return MakeEvent(invalid, ProcessState::kFailed, EINVAL, UnixTimeNs(),
                         "process name and command are required");
    }

    ManagedProcess* existing = FindByName(spec.name);
    // runtime/v1 cannot express orchestration fields. Preserve the launch
    // contract when starting an existing configured process through that API.
    if (existing) {
        spec.depends_on = existing->spec.depends_on;
        spec.notify_ready = existing->spec.notify_ready;
        spec.startup_timeout = existing->spec.startup_timeout;
        spec.shutdown_timeout = existing->spec.shutdown_timeout;
        spec.heartbeat_timeout = existing->spec.heartbeat_timeout;
    }
    std::vector<ProcessSpec> graph;
    for (const auto& p : processes_) if (p.spec.name != spec.name) graph.push_back(p.spec);
    graph.push_back(spec);
    try { ValidateLaunchGraph(graph); }
    catch (const std::exception& e) {
        ManagedProcess invalid; invalid.spec = spec;
        return MakeEvent(invalid, ProcessState::kFailed, EINVAL, UnixTimeNs(), e.what());
    }
    if (existing) {
        if (existing->pid > 0) {
            return MakeEvent(*existing, existing->state, 0,
                             existing->stop_time_unix_ns,
                             "process already running");
        }
        existing->spec = std::move(spec);
        existing->desired = true;
        existing->retry_at = {};
        Schedule();
        return MakeEvent(*existing, existing->state, 0,
                         existing->stop_time_unix_ns,
                         "start requested");
    }

    processes_.push_back(ManagedProcess{std::move(spec)});
    auto& process = processes_.back();
    process.desired = true;
    Schedule();
    return MakeEvent(process, process.state, 0,
                     process.stop_time_unix_ns,
                     "start requested");
}

ProcessManager::ProcessEvent ProcessManager::StopProcess(
    const std::string& name,
    pid_t pid,
    std::chrono::milliseconds timeout) {
    ManagedProcess* process = nullptr;
    if (!name.empty()) {
        process = FindByName(name);
    }
    if (!process && pid > 0) {
        process = FindByPid(pid);
    }
    if (!process) {
        ManagedProcess missing;
        missing.spec.name = name.empty() ? std::to_string(pid) : name;
        missing.pid = pid;
        missing.state = ProcessState::kFailed;
        return MakeEvent(missing, ProcessState::kFailed, ESRCH,
                         UnixTimeNs(), "process not found");
    }
    if (process->pid <= 0) {
        return MakeEvent(*process, ProcessState::kStopped, 0,
                         process->stop_time_unix_ns,
                         "process already stopped");
    }

    process->desired = false;
    StopDependents(process->spec.name);
    StopOne(*process, "stop requested", timeout);
    return MakeEvent(*process, ProcessState::kStopping, 0, 0, "stop requested");
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
    process.intentional_stop = false;
    process.supervision_failure = false;
    EmitEvent(process, process.state, 0, 0, "starting");

    int channel[2] = {-1, -1};
    if (process.spec.notify_ready && socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC | SOCK_NONBLOCK, 0, channel) != 0) {
        process.state = ProcessState::kFailed;
        EmitEvent(process, process.state, errno, UnixTimeNs(), "readiness socket failed");
        return false;
    }
    const pid_t pid = fork();
    if (pid < 0) {
        if (channel[0] >= 0) { close(channel[0]); close(channel[1]); }
        LOG_E("fork failed for %s: %s", process.spec.name.c_str(), strerror(errno));
        process.state = ProcessState::kFailed;
        EmitEvent(process, process.state, errno, UnixTimeNs(), strerror(errno));
        return false;
    }

    if (pid == 0) {
        if (channel[0] >= 0) {
            close(channel[0]);
            if (fcntl(channel[1], F_SETFD, 0) != 0) _exit(127);
        }

        if (!process.spec.working_directory.empty() &&
            chdir(process.spec.working_directory.c_str()) != 0) {
            perror("chdir");
            _exit(127);
        }

        for (const auto& [key, value] : process.spec.env) {
            setenv(key.c_str(), value.c_str(), 1);
        }

        if (channel[1] >= 0) setenv("OPENEMBER_READY_FD", std::to_string(channel[1]).c_str(), 1);
        else unsetenv("OPENEMBER_READY_FD");
        auto argv = BuildArgv(process.spec);
        if (process.spec.command.find('/') != std::string::npos) {
            execv(process.spec.command.c_str(), argv.data());
        } else {
            execvp(process.spec.command.c_str(), argv.data());
        }
        perror("exec");
        _exit(127);
    }

    if (channel[1] >= 0) close(channel[1]);
    process.ready_fd = channel[0];
    process.pid = pid;
    process.ready = !process.spec.notify_ready;
    process.state = process.ready ? ProcessState::kRunning : ProcessState::kStarting;
    process.start_time = std::chrono::steady_clock::now();
    LOG_I("Started process: name=%s pid=%d", process.spec.name.c_str(), static_cast<int>(pid));
    EmitEvent(process, process.state, 0, 0, process.ready ? "process created (not application readiness)" : "waiting for readiness");
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
            CheckReadiness();
            CheckHeartbeatTimeouts();
            CheckStopDeadlines();
            Schedule();
            return;
        }
        if (errno == EINTR) {
            continue;
        }
        if (errno != ECHILD) {
            LOG_E("waitpid failed: %s", strerror(errno));
        }
        CheckReadiness();
        CheckHeartbeatTimeouts();
        CheckStopDeadlines();
        Schedule();
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
    const auto state = (stopping_ || process->intentional_stop)
        ? ProcessState::kStopped
        : (ExitedSuccessfully(status) && !process->supervision_failure ? ProcessState::kExited : ProcessState::kFailed);
    const bool intentional_stop = process->intentional_stop;

    process->state = state;
    process->stop_time_unix_ns = stop_time_unix_ns;
    EmitEvent(*process, state, exit_code, stop_time_unix_ns, StatusText(status));
    process->pid = -1;
    process->ready = false;
    if (process->ready_fd >= 0) { close(process->ready_fd); process->ready_fd = -1; }
    StopDependents(process->spec.name);
    process->intentional_stop = false;

    if (!stopping_ && !intentional_stop && ShouldRestart(*process, status)) {
        ++process->restart_count;
        LOG_I("Restart process: name=%s restart_count=%u",
              process->spec.name.c_str(),
              process->restart_count);
        const auto delay = std::min<std::uint32_t>(5000, 100u << std::min<std::uint32_t>(process->restart_count - 1, 6));
        process->retry_at = std::chrono::steady_clock::now() + std::chrono::milliseconds(delay);
        process->desired = true;
    } else if (!intentional_stop) {
        process->desired = false;
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
    return process.supervision_failure || !ExitedSuccessfully(status);
}

void ProcessManager::StopAll() {
    stopping_ = true;
    for (auto& process : processes_) process.desired = false;
    // Stop leaves and wait for them to exit before stopping their providers.
    for (;;) {
        Poll();
        bool alive = false;
        for (auto& process : processes_) {
            if (process.pid <= 0) continue;
            alive = true;
            bool has_consumer = false;
            for (const auto& other : processes_)
                if (other.pid > 0 && DependsOn(other, process.spec.name)) has_consumer = true;
            if (!has_consumer && process.state != ProcessState::kStopping)
                StopOne(process);
        }
        if (!alive) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
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

void ProcessManager::StopOne(ManagedProcess& process,
                             const std::string& message,
                             std::chrono::milliseconds timeout,
                             bool intentional_stop) {
    if (process.pid <= 0) {
        return;
    }

    LOG_I("Stop process: name=%s pid=%d",
          process.spec.name.c_str(),
          static_cast<int>(process.pid));
    process.ready = false;
    StopDependents(process.spec.name);
    process.state = ProcessState::kStopping;
    process.intentional_stop = intentional_stop;
    process.supervision_failure = !intentional_stop;
    process.stop_deadline =
        std::chrono::steady_clock::now() +
        (timeout.count() > 0 ? timeout : process.spec.shutdown_timeout);
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

        // heartbeat_timeout_ms == 0 means the process is not heartbeat-supervised.
        // Do not require a startup heartbeat in that case (e.g. web_console).
        if (process.spec.heartbeat_timeout.count() > 0 &&
            !process.has_heartbeat &&
            process.spec.startup_timeout.count() > 0 &&
            now - process.start_time > process.spec.startup_timeout) {
            LOG_E("Startup heartbeat timeout: name=%s pid=%d",
                  process.spec.name.c_str(),
                  static_cast<int>(process.pid));
            StopOne(process,
                    "startup heartbeat timeout",
                    std::chrono::milliseconds{0},
                    false);
            continue;
        }

        if (process.has_heartbeat &&
            process.spec.heartbeat_timeout.count() > 0 &&
            now - process.last_heartbeat_time > process.spec.heartbeat_timeout) {
            LOG_E("Heartbeat timeout: name=%s pid=%d",
                  process.spec.name.c_str(),
                  static_cast<int>(process.pid));
            StopOne(process,
                    "heartbeat timeout",
                    std::chrono::milliseconds{0},
                    false);
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

ProcessManager::ManagedProcess* ProcessManager::FindByName(const std::string& name) {
    for (auto& process : processes_) {
        if (process.spec.name == name) {
            return &process;
        }
    }
    return nullptr;
}

ProcessManager::ProcessEvent ProcessManager::MakeEvent(
    const ManagedProcess& process,
    ProcessState state,
    int exit_code,
    std::uint64_t stop_time_unix_ns,
    const std::string& message) const {
    ProcessEvent event;
    event.spec = process.spec;
    event.pid = process.pid;
    event.state = state;
    event.exit_code = exit_code;
    event.start_time_unix_ns = process.start_time_unix_ns;
    event.stop_time_unix_ns = stop_time_unix_ns;
    event.message = message;
    event.ready = process.ready && state == ProcessState::kRunning;
    event.desired = process.desired;
    event.restart_count = process.restart_count;
    event.heartbeat_received = process.has_heartbeat;
    if (process.has_heartbeat)
        event.heartbeat_age_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - process.last_heartbeat_time).count();
    return event;
}

std::vector<ProcessManager::ProcessEvent> ProcessManager::Snapshot() const {
    std::vector<ProcessEvent> result;
    result.reserve(processes_.size());
    for (const auto& process : processes_)
        result.push_back(MakeEvent(process, process.state, process.last_exit_code,
                                   process.stop_time_unix_ns, process.last_message));
    return result;
}

void ProcessManager::EmitEvent(ManagedProcess& process,
                               ProcessState state,
                               int exit_code,
                               std::uint64_t stop_time_unix_ns,
                               const std::string& message) const {
    process.last_exit_code = exit_code;
    process.stop_time_unix_ns = stop_time_unix_ns;
    process.last_message = message;
    if (!event_callback_) {
        return;
    }

    const ProcessEvent event =
        MakeEvent(process, state, exit_code, stop_time_unix_ns, message);
    event_callback_(event);
}

}  // namespace openember::launch
