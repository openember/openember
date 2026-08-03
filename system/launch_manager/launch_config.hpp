#pragma once

#include <chrono>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace openember::launch {

enum class RestartPolicy {
    kNever,
    kOnFailure,
    kAlways,
};

struct RuntimeSpec {
    std::string robot_id = "openember";
    std::string namespace_name;
    std::string listen_endpoint = "tcp/127.0.0.1:7447";
};

struct ProcessSpec {
    std::string name;
    std::string command;
    std::vector<std::string> args;
    std::map<std::string, std::string> env;
    std::string working_directory;
    RestartPolicy restart = RestartPolicy::kNever;
    std::uint32_t restart_limit = 0;
    std::chrono::milliseconds startup_timeout{3000};
    std::chrono::milliseconds shutdown_timeout{3000};
    std::chrono::milliseconds heartbeat_timeout{0};
};

struct LaunchConfig {
    RuntimeSpec runtime;
    std::vector<ProcessSpec> processes;
};

LaunchConfig LoadLaunchConfig(const std::string& path,
                              const std::string& openember_bin_dir);

const char* RestartPolicyName(RestartPolicy policy);

}  // namespace openember::launch
