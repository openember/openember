/*
 * Copyright (c) 2022-2026, OpenEmber Team
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <fcntl.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#define APPLICATION_NAME "launch_manager"
#define LOG_TAG APPLICATION_NAME
#include "openember.h"

#include "launch_config.hpp"
#include "process_manager.hpp"

#include "openember/init.hpp"
#include "openember/link/options.hpp"
#include "openember/msgs/node/v1/node.pb.h"
#include "openember/msgs/runtime/v1/runtime.pb.h"
#include "openember/node.hpp"

namespace {

constexpr const char* kDefaultPidFile = "/tmp/openember-launch_manager.pid";
constexpr const char* kDefaultLaunchFile = "configs/smart_device.launch.yaml";
constexpr const char* kRuntimeProcessEventsTopic = "/runtime/process/events";
constexpr const char* kNodeHeartbeatsTopic = "/nodes/*/heartbeat";

volatile sig_atomic_t g_signal = 0;
pid_t g_pid = 0;
int g_lock_fd = -1;

void SignalHandler(int signo) {
    g_signal = signo;
}

std::string EnvOrDefault(const char* name, const char* fallback) {
    const char* value = std::getenv(name);
    if (value && *value) {
        return value;
    }
    return fallback;
}

std::uint64_t UnixTimeNs() {
    const auto now = std::chrono::system_clock::now().time_since_epoch();
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(now).count());
}

std::string ExecutablePath() {
    char path[4096] = {0};
    const ssize_t n = readlink("/proc/self/exe", path, sizeof(path) - 1);
    if (n <= 0) {
        return {};
    }
    path[n] = '\0';
    return path;
}

std::string ExecutableDir() {
    const std::string exe = ExecutablePath();
    if (exe.empty()) {
        return ".";
    }
    return std::filesystem::path(exe).parent_path().string();
}

std::string ResolveDefaultLaunchFile(const std::string& bin_dir) {
    namespace fs = std::filesystem;

    const std::string env_path = EnvOrDefault("OPENEMBER_LAUNCH_FILE", "");
    if (!env_path.empty()) {
        return env_path;
    }

    if (fs::exists(kDefaultLaunchFile)) {
        return kDefaultLaunchFile;
    }

    const fs::path source_tree_candidate =
        fs::path(bin_dir) / ".." / ".." / kDefaultLaunchFile;
    if (fs::exists(source_tree_candidate)) {
        return source_tree_candidate.lexically_normal().string();
    }

    const fs::path install_candidate =
        fs::path(bin_dir) / ".." / "share" / "openember" / kDefaultLaunchFile;
    if (fs::exists(install_candidate)) {
        return install_candidate.lexically_normal().string();
    }

    return kDefaultLaunchFile;
}

void PrintUsage(const char* argv0) {
    std::printf(
        "Usage:\n"
        "  %s [--launch <file>] [--pid-file <file>]\n"
        "  %s stop [--pid-file <file>]\n",
        argv0,
        argv0);
}

struct CliOptions {
    bool stop = false;
    bool help = false;
    std::string launch_file;
    std::string pid_file = EnvOrDefault("OPENEMBER_PID_FILE", kDefaultPidFile);
};

CliOptions ParseArgs(int argc, char* argv[], const std::string& bin_dir) {
    CliOptions options;
    options.launch_file = ResolveDefaultLaunchFile(bin_dir);

    for (int i = 1; i < argc; ++i) {
        const std::string arg(argv[i]);
        if (arg == "stop") {
            options.stop = true;
        } else if (arg == "--help" || arg == "-h") {
            options.help = true;
        } else if (arg == "--launch" && i + 1 < argc) {
            options.launch_file = argv[++i];
        } else if (arg == "--pid-file" && i + 1 < argc) {
            options.pid_file = argv[++i];
        } else {
            std::fprintf(stderr, "unknown argument: %s\n", arg.c_str());
            options.help = true;
        }
    }

    return options;
}

int CreateLockFile(const std::string& lockfile) {
    g_lock_fd = open(lockfile.c_str(), O_WRONLY | O_CREAT, 0666);
    if (g_lock_fd < 0) {
        perror("Fail to open pid file");
        return -EMBER_ENOFILE;
    }

    struct flock lock {};
    lock.l_type = F_WRLCK;
    lock.l_whence = SEEK_SET;

    if (fcntl(g_lock_fd, F_SETLK, &lock) < 0) {
        perror("Fail to fcntl F_SETLK");
        close(g_lock_fd);
        g_lock_fd = -1;
        LOG_I("%s has been running", APPLICATION_NAME);
        return -EMBER_ENOPERM;
    }

    char buf[32] = {0};
    g_pid = getpid();
    const int len = snprintf(buf, sizeof(buf), "%d\n", static_cast<int>(g_pid));
    (void)ftruncate(g_lock_fd, 0);
    (void)write(g_lock_fd, buf, len);
    return EMBER_EOK;
}

void DestroyLockFile(const std::string& lockfile) {
    if (g_lock_fd >= 0) {
        close(g_lock_fd);
        g_lock_fd = -1;
    }
    (void)unlink(lockfile.c_str());
}

pid_t ReadPidFile(const std::string& lockfile) {
    const int fd = open(lockfile.c_str(), O_RDONLY, 0644);
    if (fd < 0) {
        perror("Fail to open pid file");
        return -1;
    }

    char buf[32] = {0};
    const ssize_t ret = read(fd, buf, sizeof(buf) - 1);
    close(fd);
    if (ret <= 0) {
        return -1;
    }
    return static_cast<pid_t>(std::atoi(buf));
}

void InitLinkRouter(const openember::launch::RuntimeSpec& runtime) {
    openember::RuntimeOptions options;
    options.robot_id = runtime.robot_id;
    options.namespace_name = runtime.namespace_name;
    options.link.profile = openember::link::Profile::kRouter;
    options.link.listen = runtime.listen_endpoint;
    openember::Init(options);
}

openember::msgs::runtime::v1::ProcessState ToProtoState(
    openember::launch::ProcessManager::ProcessState state) {
    using LocalState = openember::launch::ProcessManager::ProcessState;
    using ProtoState = openember::msgs::runtime::v1::ProcessState;

    switch (state) {
        case LocalState::kStarting:
            return ProtoState::PROCESS_STATE_STARTING;
        case LocalState::kRunning:
            return ProtoState::PROCESS_STATE_RUNNING;
        case LocalState::kStopping:
            return ProtoState::PROCESS_STATE_STOPPING;
        case LocalState::kStopped:
            return ProtoState::PROCESS_STATE_STOPPED;
        case LocalState::kExited:
            return ProtoState::PROCESS_STATE_EXITED;
        case LocalState::kFailed:
            return ProtoState::PROCESS_STATE_FAILED;
    }
    return ProtoState::PROCESS_STATE_UNSPECIFIED;
}

openember::msgs::runtime::v1::ProcessEvent BuildProcessEvent(
    const openember::launch::RuntimeSpec& runtime,
    const openember::launch::ProcessManager::ProcessEvent& source,
    std::uint64_t sequence) {
    openember::msgs::runtime::v1::ProcessEvent event;

    auto* header = event.mutable_header();
    header->set_source_node(APPLICATION_NAME);
    header->set_source_instance(APPLICATION_NAME);
    header->set_robot_id(runtime.robot_id);
    header->set_namespace_name(runtime.namespace_name);
    header->set_sequence(sequence);
    header->set_timestamp_unix_ns(UnixTimeNs());

    auto* process = event.mutable_process();
    process->set_name(source.spec.name);
    if (source.pid > 0) {
        process->set_process_id(static_cast<std::uint32_t>(source.pid));
    }
    process->set_state(ToProtoState(source.state));
    process->set_exit_code(source.exit_code);
    process->set_start_time_unix_ns(source.start_time_unix_ns);
    process->set_stop_time_unix_ns(source.stop_time_unix_ns);
    process->set_message(source.message);
    return event;
}

void InstallSignals() {
    signal(SIGHUP, SignalHandler);
    signal(SIGINT, SignalHandler);
    signal(SIGTERM, SignalHandler);
    signal(SIGQUIT, SignalHandler);
}

}  // namespace

int main(int argc, char* argv[]) {
    const std::string bin_dir = ExecutableDir();
    const CliOptions cli = ParseArgs(argc, argv, bin_dir);

    if (cli.help) {
        PrintUsage(argv[0]);
        return cli.stop ? 1 : 0;
    }

    if (cli.stop) {
        const pid_t pid = ReadPidFile(cli.pid_file);
        if (pid <= 0) {
            std::fprintf(stderr, "launch_manager is not running: %s\n", cli.pid_file.c_str());
            return 1;
        }
        if (kill(pid, SIGTERM) != 0) {
            perror("Fail to stop launch_manager");
            return 1;
        }
        return 0;
    }

    log_init(APPLICATION_NAME);
    LOG_I("Start openember app, version: %lu.%lu.%lu", EMBER_VERSION, EMBER_SUBVERSION,
          EMBER_REVISION);
    LOG_I("Process id is %d", getpid());
    LOG_I("Launch file: %s", cli.launch_file.c_str());

    if (CreateLockFile(cli.pid_file) != EMBER_EOK) {
        LOG_E("Can not create lock file: %s", cli.pid_file.c_str());
        return 1;
    }

    InstallSignals();

    int exit_code = 0;

    try {
        auto config = openember::launch::LoadLaunchConfig(cli.launch_file, bin_dir);
        InitLinkRouter(config.runtime);
        auto node = openember::CreateNode(APPLICATION_NAME);
        auto process_event_pub =
            node->Advertise<openember::msgs::runtime::v1::ProcessEvent>(
                kRuntimeProcessEventsTopic);
        std::mutex heartbeat_mutex;
        std::vector<std::string> pending_heartbeats;
        auto heartbeat_sub =
            node->Subscribe<openember::msgs::node::v1::NodeHeartbeat>(
                kNodeHeartbeatsTopic,
                [&](const openember::msgs::node::v1::NodeHeartbeat& heartbeat) {
                    std::lock_guard<std::mutex> lock(heartbeat_mutex);
                    pending_heartbeats.push_back(heartbeat.node_name());
                });
        std::uint64_t process_event_sequence = 0;

        openember::launch::ProcessManager process_manager(config.processes);
        process_manager.SetEventCallback(
            [&](const openember::launch::ProcessManager::ProcessEvent& event) {
                auto msg = BuildProcessEvent(
                    config.runtime, event, process_event_sequence++);
                if (!process_event_pub.Publish(msg)) {
                    LOG_E("Publish ProcessEvent failed: name=%s state=%d",
                          event.spec.name.c_str(),
                          static_cast<int>(event.state));
                }
            });
        if (!process_manager.StartAll()) {
            LOG_E("Some configured processes failed to start.");
        }

        while (g_signal == 0) {
            std::vector<std::string> heartbeats;
            {
                std::lock_guard<std::mutex> lock(heartbeat_mutex);
                heartbeats.swap(pending_heartbeats);
            }
            for (const auto& node_name : heartbeats) {
                (void)process_manager.MarkHeartbeat(node_name);
            }
            process_manager.Poll();
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        }

        LOG_I("Stopping launch_manager on signal %d", static_cast<int>(g_signal));
        process_manager.StopAll();
    } catch (const std::exception& e) {
        LOG_E("launch_manager failed: %s", e.what());
        exit_code = 1;
    }

    openember::Shutdown();
    DestroyLockFile(cli.pid_file);
    log_deinit();
    return exit_code;
}
