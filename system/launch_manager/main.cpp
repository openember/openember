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

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <functional>
#include <future>
#include <filesystem>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
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
constexpr const char* kRuntimeProcessStartService = "/runtime/process/start";
constexpr const char* kRuntimeProcessStopService = "/runtime/process/stop";
constexpr const char* kNodeInfoTopic = "/nodes/*/info";
constexpr const char* kNodeHeartbeatsTopic = "/nodes/*/heartbeat";
constexpr const char* kNodeQueryService = "/nodes/query";

volatile sig_atomic_t g_signal = 0;
pid_t g_pid = 0;
int g_lock_fd = -1;

std::uint64_t UnixTimeNs();

class RuntimeRegistry {
public:
    void UpdateNodeInfo(const openember::msgs::node::v1::NodeInfo& info) {
        if (info.node_name().empty()) {
            return;
        }

        std::lock_guard<std::mutex> lock(mutex_);
        auto& record = nodes_[info.node_name()];
        const bool first_seen = record.info.node_name().empty();
        record.info = info;
        record.last_info_unix_ns = UnixTimeNs();

        if (first_seen) {
            LOG_I("Node registered: name=%s kind=%d pid=%u",
                  info.node_name().c_str(),
                  static_cast<int>(info.kind()),
                  info.process_id());
        }
    }

    void UpdateHeartbeat(const openember::msgs::node::v1::NodeHeartbeat& heartbeat) {
        if (heartbeat.node_name().empty()) {
            return;
        }

        std::lock_guard<std::mutex> lock(mutex_);
        auto& record = nodes_[heartbeat.node_name()];
        if (record.info.node_name().empty()) {
            record.info.set_node_name(heartbeat.node_name());
            record.info.set_instance_id(heartbeat.instance_id());
            record.info.set_kind(openember::msgs::node::v1::NODE_KIND_UNSPECIFIED);
        }
        record.last_heartbeat_unix_ns = UnixTimeNs();
    }

    std::vector<openember::msgs::node::v1::NodeInfo> Query(
        const openember::msgs::node::v1::NodeQuery& query) const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<openember::msgs::node::v1::NodeInfo> result;

        for (const auto& [name, record] : nodes_) {
            if (!query.node_name().empty() && query.node_name() != name) {
                continue;
            }
            if (!query.namespace_name().empty() &&
                query.namespace_name() != record.info.namespace_name()) {
                continue;
            }

            auto info = record.info;
            if (!query.include_endpoints()) {
                info.clear_topics();
                info.clear_services();
            }
            result.push_back(std::move(info));
        }

        return result;
    }

private:
    struct NodeRecord {
        openember::msgs::node::v1::NodeInfo info;
        std::uint64_t last_info_unix_ns = 0;
        std::uint64_t last_heartbeat_unix_ns = 0;
    };

    mutable std::mutex mutex_;
    std::map<std::string, NodeRecord> nodes_;
};

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

void FillHeader(openember::msgs::common::v1::Header* header,
                const openember::launch::RuntimeSpec& runtime,
                const openember::msgs::common::v1::Header* request_header,
                std::uint64_t sequence) {
    header->set_source_node(APPLICATION_NAME);
    header->set_source_instance(APPLICATION_NAME);
    header->set_robot_id(runtime.robot_id);
    header->set_namespace_name(runtime.namespace_name);
    header->set_sequence(sequence);
    header->set_timestamp_unix_ns(UnixTimeNs());
    if (request_header) {
        header->set_trace_id(request_header->trace_id());
        header->set_request_id(request_header->request_id());
    }
}

void FillProcessInfo(
    openember::msgs::runtime::v1::ProcessInfo* process,
    const openember::launch::ProcessManager::ProcessEvent& source) {
    process->set_name(source.spec.name);
    if (source.pid > 0) {
        process->set_process_id(static_cast<std::uint32_t>(source.pid));
    }
    process->set_state(ToProtoState(source.state));
    process->set_exit_code(source.exit_code);
    process->set_start_time_unix_ns(source.start_time_unix_ns);
    process->set_stop_time_unix_ns(source.stop_time_unix_ns);
    process->set_message(source.message);
}

bool IsProcessCommandOk(const openember::launch::ProcessManager::ProcessEvent& event) {
    return event.state != openember::launch::ProcessManager::ProcessState::kFailed;
}

void FillStatus(openember::msgs::common::v1::Status* status,
                bool ok,
                const std::string& message) {
    status->set_code(ok ? 0 : 1);
    status->set_message(message);
}

openember::launch::ProcessSpec ToLaunchProcessSpec(
    const openember::msgs::runtime::v1::ProcessSpec& source) {
    openember::launch::ProcessSpec spec;
    spec.name = source.name();
    spec.command = source.executable();
    spec.args.assign(source.args().begin(), source.args().end());
    for (const auto& env : source.env()) {
        spec.env[env.key()] = env.value();
    }
    spec.working_directory = source.working_directory();
    spec.restart = source.restart_on_failure()
        ? openember::launch::RestartPolicy::kOnFailure
        : openember::launch::RestartPolicy::kNever;
    spec.restart_limit = source.restart_limit();
    return spec;
}

openember::msgs::runtime::v1::StartProcessResponse BuildStartProcessResponse(
    const openember::launch::RuntimeSpec& runtime,
    const openember::msgs::runtime::v1::StartProcessRequest& request,
    const openember::launch::ProcessManager::ProcessEvent& event,
    std::uint64_t sequence) {
    openember::msgs::runtime::v1::StartProcessResponse response;
    FillHeader(response.mutable_header(), runtime, &request.header(), sequence);
    FillStatus(response.mutable_status(), IsProcessCommandOk(event), event.message);
    FillProcessInfo(response.mutable_process(), event);
    return response;
}

openember::msgs::runtime::v1::StopProcessResponse BuildStopProcessResponse(
    const openember::launch::RuntimeSpec& runtime,
    const openember::msgs::runtime::v1::StopProcessRequest& request,
    const openember::launch::ProcessManager::ProcessEvent& event,
    std::uint64_t sequence) {
    openember::msgs::runtime::v1::StopProcessResponse response;
    FillHeader(response.mutable_header(), runtime, &request.header(), sequence);
    FillStatus(response.mutable_status(), IsProcessCommandOk(event), event.message);
    FillProcessInfo(response.mutable_process(), event);
    return response;
}

openember::msgs::runtime::v1::StartProcessResponse BuildStartTimeoutResponse(
    const openember::launch::RuntimeSpec& runtime,
    const openember::msgs::runtime::v1::StartProcessRequest& request,
    std::uint64_t sequence) {
    openember::msgs::runtime::v1::StartProcessResponse response;
    FillHeader(response.mutable_header(), runtime, &request.header(), sequence);
    FillStatus(response.mutable_status(), false, "runtime command timeout");
    response.mutable_process()->set_name(request.process().name());
    response.mutable_process()->set_state(
        openember::msgs::runtime::v1::PROCESS_STATE_FAILED);
    return response;
}

openember::msgs::runtime::v1::StopProcessResponse BuildStopTimeoutResponse(
    const openember::launch::RuntimeSpec& runtime,
    const openember::msgs::runtime::v1::StopProcessRequest& request,
    std::uint64_t sequence) {
    openember::msgs::runtime::v1::StopProcessResponse response;
    FillHeader(response.mutable_header(), runtime, &request.header(), sequence);
    FillStatus(response.mutable_status(), false, "runtime command timeout");
    response.mutable_process()->set_name(request.name());
    response.mutable_process()->set_process_id(request.process_id());
    response.mutable_process()->set_state(
        openember::msgs::runtime::v1::PROCESS_STATE_FAILED);
    return response;
}

openember::msgs::node::v1::NodeQueryResponse BuildNodeQueryResponse(
    const openember::launch::RuntimeSpec& runtime,
    const openember::msgs::node::v1::NodeQuery& request,
    const std::vector<openember::msgs::node::v1::NodeInfo>& nodes,
    std::uint64_t sequence) {
    openember::msgs::node::v1::NodeQueryResponse response;
    FillHeader(response.mutable_header(), runtime, &request.header(), sequence);
    FillStatus(response.mutable_status(), true, "ok");
    for (const auto& node : nodes) {
        *response.add_nodes() = node;
    }
    return response;
}

openember::msgs::runtime::v1::ProcessEvent BuildProcessEvent(
    const openember::launch::RuntimeSpec& runtime,
    const openember::launch::ProcessManager::ProcessEvent& source,
    std::uint64_t sequence) {
    openember::msgs::runtime::v1::ProcessEvent event;
    FillHeader(event.mutable_header(), runtime, nullptr, sequence);
    FillProcessInfo(event.mutable_process(), source);
    return event;
}

template <typename ResponseT, typename WorkT>
ResponseT EnqueueRuntimeCommand(std::mutex& mutex,
                                std::vector<std::function<void()>>& pending_commands,
                                WorkT&& work,
                                ResponseT timeout_response) {
    auto promise = std::make_shared<std::promise<ResponseT>>();
    auto future = promise->get_future();
    {
        std::lock_guard<std::mutex> lock(mutex);
        pending_commands.push_back(
            [promise, work = std::forward<WorkT>(work)]() mutable {
                try {
                    promise->set_value(work());
                } catch (...) {
                    promise->set_exception(std::current_exception());
                }
            });
    }
    if (future.wait_for(std::chrono::seconds(3)) != std::future_status::ready) {
        return timeout_response;
    }
    return future.get();
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
        RuntimeRegistry registry;
        std::mutex heartbeat_mutex;
        std::vector<std::string> pending_heartbeats;
        std::mutex runtime_command_mutex;
        std::vector<std::function<void()>> pending_runtime_commands;
        std::atomic_uint64_t runtime_service_sequence{0};
        std::atomic_uint64_t node_query_sequence{0};
        auto node_info_sub =
            node->Subscribe<openember::msgs::node::v1::NodeInfo>(
                kNodeInfoTopic,
                [&](const openember::msgs::node::v1::NodeInfo& info) {
                    registry.UpdateNodeInfo(info);
                });
        auto heartbeat_sub =
            node->Subscribe<openember::msgs::node::v1::NodeHeartbeat>(
                kNodeHeartbeatsTopic,
                [&](const openember::msgs::node::v1::NodeHeartbeat& heartbeat) {
                    registry.UpdateHeartbeat(heartbeat);
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

        auto node_query_service = node->CreateService<
            openember::msgs::node::v1::NodeQuery,
            openember::msgs::node::v1::NodeQueryResponse>(
            kNodeQueryService,
            [&](const openember::msgs::node::v1::NodeQuery& request) {
                return BuildNodeQueryResponse(
                    config.runtime,
                    request,
                    registry.Query(request),
                    node_query_sequence.fetch_add(1));
            });

        auto start_service = node->CreateService<
            openember::msgs::runtime::v1::StartProcessRequest,
            openember::msgs::runtime::v1::StartProcessResponse>(
            kRuntimeProcessStartService,
            [&](const openember::msgs::runtime::v1::StartProcessRequest& request) {
                auto timeout_response = BuildStartTimeoutResponse(
                    config.runtime, request, runtime_service_sequence.fetch_add(1));
                return EnqueueRuntimeCommand(
                    runtime_command_mutex,
                    pending_runtime_commands,
                    [&, request]() {
                        const auto event =
                            process_manager.StartProcess(ToLaunchProcessSpec(request.process()));
                        return BuildStartProcessResponse(
                            config.runtime,
                            request,
                            event,
                            runtime_service_sequence.fetch_add(1));
                    },
                    timeout_response);
            });

        auto stop_service = node->CreateService<
            openember::msgs::runtime::v1::StopProcessRequest,
            openember::msgs::runtime::v1::StopProcessResponse>(
            kRuntimeProcessStopService,
            [&](const openember::msgs::runtime::v1::StopProcessRequest& request) {
                auto timeout_response = BuildStopTimeoutResponse(
                    config.runtime, request, runtime_service_sequence.fetch_add(1));
                return EnqueueRuntimeCommand(
                    runtime_command_mutex,
                    pending_runtime_commands,
                    [&, request]() {
                        const auto event = process_manager.StopProcess(
                            request.name(),
                            static_cast<pid_t>(request.process_id()),
                            std::chrono::milliseconds(request.timeout_ms()));
                        return BuildStopProcessResponse(
                            config.runtime,
                            request,
                            event,
                            runtime_service_sequence.fetch_add(1));
                    },
                    timeout_response);
            });

        if (!process_manager.StartAll()) {
            LOG_E("Some configured processes failed to start.");
        }

        while (g_signal == 0) {
            std::vector<std::function<void()>> runtime_commands;
            {
                std::lock_guard<std::mutex> lock(runtime_command_mutex);
                runtime_commands.swap(pending_runtime_commands);
            }
            for (auto& command : runtime_commands) {
                command();
            }

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
