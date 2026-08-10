#include <chrono>
#include <cstdint>
#include <iostream>
#include <string>

#include "openember/init.hpp"
#include "openember/link/options.hpp"
#include "openember/msgs/runtime/v1/runtime.pb.h"
#include "openember/node.hpp"

namespace {

constexpr const char* kRobotId = "openember";
constexpr const char* kNodeName = "runtime_process_control";
constexpr const char* kStartService = "/runtime/process/start";
constexpr const char* kStopService = "/runtime/process/stop";

std::uint64_t UnixTimeNs() {
    const auto now = std::chrono::system_clock::now().time_since_epoch();
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(now).count());
}

void FillHeader(openember::msgs::common::v1::Header* header,
                const std::string& request_id) {
    header->set_source_node(kNodeName);
    header->set_source_instance(kNodeName);
    header->set_robot_id(kRobotId);
    header->set_timestamp_unix_ns(UnixTimeNs());
    header->set_request_id(request_id);
}

void PrintUsage(const char* argv0) {
    std::cerr
        << "Usage:\n"
        << "  " << argv0 << " stop <name> [timeout_ms]\n"
        << "  " << argv0 << " start <name> <executable> [args...]\n";
}

int RunStop(int argc, char** argv) {
    if (argc < 3) {
        return 1;
    }

    auto node = openember::CreateNode(kNodeName);
    auto client = node->CreateClient<
        openember::msgs::runtime::v1::StopProcessRequest,
        openember::msgs::runtime::v1::StopProcessResponse>(kStopService);

    openember::msgs::runtime::v1::StopProcessRequest request;
    FillHeader(request.mutable_header(), "stop-" + std::string(argv[2]));
    request.set_name(argv[2]);
    if (argc >= 4) {
        request.set_timeout_ms(static_cast<std::uint64_t>(std::stoull(argv[3])));
    }

    const auto response = client.Call(request, std::chrono::seconds(3));
    if (!response.has_value()) {
        std::cerr << "runtime stop request failed or timed out" << std::endl;
        return 1;
    }

    const auto& status = response->status();
    const auto& process = response->process();
    std::cout << "stop status=" << status.code()
              << " message=\"" << status.message() << "\""
              << " name=" << process.name()
              << " pid=" << process.process_id()
              << " state=" << process.state()
              << std::endl;
    return status.code() == 0 ? 0 : 1;
}

int RunStart(int argc, char** argv) {
    if (argc < 4) {
        return 1;
    }

    auto node = openember::CreateNode(kNodeName);
    auto client = node->CreateClient<
        openember::msgs::runtime::v1::StartProcessRequest,
        openember::msgs::runtime::v1::StartProcessResponse>(kStartService);

    openember::msgs::runtime::v1::StartProcessRequest request;
    FillHeader(request.mutable_header(), "start-" + std::string(argv[2]));
    auto* process = request.mutable_process();
    process->set_name(argv[2]);
    process->set_executable(argv[3]);
    for (int i = 4; i < argc; ++i) {
        process->add_args(argv[i]);
    }

    const auto response = client.Call(request, std::chrono::seconds(3));
    if (!response.has_value()) {
        std::cerr << "runtime start request failed or timed out" << std::endl;
        return 1;
    }

    const auto& status = response->status();
    const auto& info = response->process();
    std::cout << "start status=" << status.code()
              << " message=\"" << status.message() << "\""
              << " name=" << info.name()
              << " pid=" << info.process_id()
              << " state=" << info.state()
              << std::endl;
    return status.code() == 0 ? 0 : 1;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        PrintUsage(argv[0]);
        return 1;
    }

    openember::RuntimeOptions options;
    options.robot_id = kRobotId;
    options.link = openember::link::LocalClientOptions();
    openember::Init(options);

    int result = 1;
    const std::string command(argv[1]);
    if (command == "stop") {
        result = RunStop(argc, argv);
    } else if (command == "start") {
        result = RunStart(argc, argv);
    } else {
        PrintUsage(argv[0]);
    }

    openember::Shutdown();
    return result;
}
