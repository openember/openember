#include <chrono>
#include <cstdint>
#include <iostream>
#include <string>

#include "openember/init.hpp"
#include "openember/link/options.hpp"
#include "openember/msgs/device/v1/device.pb.h"
#include "openember/node.hpp"

namespace {

constexpr const char* kRobotId = "openember";
constexpr const char* kNodeName = "device_query";
constexpr const char* kDeviceQueryService = "/devices/query";

std::uint64_t UnixTimeNs() {
    const auto now = std::chrono::system_clock::now().time_since_epoch();
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(now).count());
}

void FillHeader(openember::msgs::common::v1::Header* header) {
    header->set_source_node(kNodeName);
    header->set_source_instance(kNodeName);
    header->set_robot_id(kRobotId);
    header->set_timestamp_unix_ns(UnixTimeNs());
    header->set_request_id("device-query");
}

}  // namespace

int main(int argc, char** argv) {
    openember::RuntimeOptions options;
    options.robot_id = kRobotId;
    options.link = openember::link::LocalClientOptions();
    openember::Init(options);

    auto node = openember::CreateNode(kNodeName);
    auto client = node->CreateClient<
        openember::msgs::device::v1::DeviceQuery,
        openember::msgs::device::v1::DeviceQueryResponse>(kDeviceQueryService);

    openember::msgs::device::v1::DeviceQuery request;
    FillHeader(request.mutable_header());
    request.set_include_state(true);
    if (argc >= 2) {
        request.set_device_id(argv[1]);
    }

    const auto response = client.Call(request, std::chrono::seconds(3));
    if (!response.has_value()) {
        std::cerr << "device query failed or timed out" << std::endl;
        openember::Shutdown();
        return 1;
    }

    std::cout << "device query status=" << response->status().code()
              << " message=\"" << response->status().message() << "\""
              << " devices=" << response->devices_size()
              << " states=" << response->states_size()
              << std::endl;

    for (const auto& info : response->devices()) {
        std::cout << "device id=" << info.device_id()
                  << " name=\"" << info.name() << "\""
                  << " category=" << info.category()
                  << " driver=" << info.driver()
                  << " path=" << info.path()
                  << std::endl;
    }
    for (const auto& state : response->states()) {
        std::cout << "state device_id=" << state.device_id()
                  << " availability=" << state.availability()
                  << " health=" << state.health()
                  << " message=\"" << state.message() << "\""
                  << std::endl;
    }

    openember::Shutdown();
    return response->status().code() == 0 ? 0 : 1;
}
