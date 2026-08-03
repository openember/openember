#include "launch_config.hpp"

#include <stdexcept>

#include <yaml-cpp/yaml.h>

namespace openember::launch {
namespace {

std::string StringValue(const YAML::Node& node,
                        const char* key,
                        const std::string& fallback = {}) {
    if (!node || !node[key]) {
        return fallback;
    }
    return node[key].as<std::string>();
}

std::uint32_t UIntValue(const YAML::Node& node,
                        const char* key,
                        std::uint32_t fallback) {
    if (!node || !node[key]) {
        return fallback;
    }
    return node[key].as<std::uint32_t>();
}

std::string ReplaceAll(std::string value,
                       const std::string& from,
                       const std::string& to) {
    std::string::size_type pos = 0;
    while ((pos = value.find(from, pos)) != std::string::npos) {
        value.replace(pos, from.size(), to);
        pos += to.size();
    }
    return value;
}

std::string ExpandValue(const std::string& value,
                        const std::string& openember_bin_dir) {
    return ReplaceAll(value, "${openember.bin}", openember_bin_dir);
}

RestartPolicy ParseRestartPolicy(const std::string& value) {
    if (value == "always") {
        return RestartPolicy::kAlways;
    }
    if (value == "on-failure" || value == "on_failure") {
        return RestartPolicy::kOnFailure;
    }
    if (value == "never" || value.empty()) {
        return RestartPolicy::kNever;
    }
    throw std::runtime_error("unsupported restart policy: " + value);
}

std::vector<std::string> ParseArgs(const YAML::Node& node,
                                   const std::string& openember_bin_dir) {
    std::vector<std::string> args;
    if (!node || !node["args"]) {
        return args;
    }

    for (const auto& arg : node["args"]) {
        args.push_back(ExpandValue(arg.as<std::string>(), openember_bin_dir));
    }
    return args;
}

std::map<std::string, std::string> ParseEnv(const YAML::Node& node,
                                            const std::string& openember_bin_dir) {
    std::map<std::string, std::string> env;
    if (!node || !node["env"]) {
        return env;
    }

    const auto env_node = node["env"];
    if (!env_node.IsMap()) {
        throw std::runtime_error("process env must be a map");
    }

    for (const auto& item : env_node) {
        env[item.first.as<std::string>()] =
            ExpandValue(item.second.as<std::string>(), openember_bin_dir);
    }
    return env;
}

}  // namespace

const char* RestartPolicyName(RestartPolicy policy) {
    switch (policy) {
        case RestartPolicy::kAlways:
            return "always";
        case RestartPolicy::kOnFailure:
            return "on-failure";
        case RestartPolicy::kNever:
        default:
            return "never";
    }
}

LaunchConfig LoadLaunchConfig(const std::string& path,
                              const std::string& openember_bin_dir) {
    YAML::Node root;
    try {
        root = YAML::LoadFile(path);
    } catch (const YAML::Exception& e) {
        throw std::runtime_error("failed to load launch file '" + path + "': " + e.what());
    }

    LaunchConfig config;

    const auto runtime = root["runtime"];
    config.runtime.robot_id = StringValue(runtime, "robot_id", config.runtime.robot_id);
    config.runtime.namespace_name = StringValue(runtime, "namespace", config.runtime.namespace_name);
    if (runtime && runtime["link"]) {
        config.runtime.listen_endpoint =
            StringValue(runtime["link"], "listen", config.runtime.listen_endpoint);
    }

    const auto nodes = root["nodes"];
    if (!nodes || !nodes.IsSequence()) {
        throw std::runtime_error("launch file must contain a 'nodes' sequence");
    }

    for (const auto& node : nodes) {
        ProcessSpec spec;
        spec.name = StringValue(node, "name");
        spec.command = ExpandValue(StringValue(node, "command"), openember_bin_dir);
        spec.args = ParseArgs(node, openember_bin_dir);
        spec.env = ParseEnv(node, openember_bin_dir);
        spec.working_directory =
            ExpandValue(StringValue(node, "working_directory"), openember_bin_dir);
        spec.restart = ParseRestartPolicy(StringValue(node, "restart", "never"));
        spec.restart_limit = UIntValue(node, "restart_limit", spec.restart_limit);
        spec.startup_timeout =
            std::chrono::milliseconds(UIntValue(node, "startup_timeout_ms", 3000));
        spec.shutdown_timeout =
            std::chrono::milliseconds(UIntValue(node, "shutdown_timeout_ms", 3000));
        spec.heartbeat_timeout =
            std::chrono::milliseconds(UIntValue(node, "heartbeat_timeout_ms", 0));

        if (spec.name.empty()) {
            throw std::runtime_error("launch node is missing required field: name");
        }
        if (spec.command.empty()) {
            throw std::runtime_error("launch node '" + spec.name +
                                     "' is missing required field: command");
        }

        config.processes.push_back(std::move(spec));
    }

    return config;
}

}  // namespace openember::launch
