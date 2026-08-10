#include <chrono>
#include <cstdint>
#include <iostream>
#include <string>

#include "openember/init.hpp"
#include "openember/link/options.hpp"
#include "openember/msgs/node/v1/node.pb.h"
#include "openember/node.hpp"

namespace {

constexpr const char* kRobotId = "openember";
constexpr const char* kNodeName = "node_query";
constexpr const char* kNodeQueryService = "/nodes/query";

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
    header->set_request_id("node-query");
}

}  // namespace

int main(int argc, char** argv) {
    openember::RuntimeOptions options;
    options.robot_id = kRobotId;
    options.link = openember::link::LocalClientOptions();
    openember::Init(options);

    auto node = openember::CreateNode(kNodeName);
    auto client = node->CreateClient<
        openember::msgs::node::v1::NodeQuery,
        openember::msgs::node::v1::NodeQueryResponse>(kNodeQueryService);

    openember::msgs::node::v1::NodeQuery request;
    FillHeader(request.mutable_header());
    request.set_include_endpoints(true);
    if (argc >= 2) {
        request.set_node_name(argv[1]);
    }

    const auto response = client.Call(request, std::chrono::seconds(3));
    if (!response.has_value()) {
        std::cerr << "node query failed or timed out" << std::endl;
        openember::Shutdown();
        return 1;
    }

    std::cout << "node query status=" << response->status().code()
              << " message=\"" << response->status().message() << "\""
              << " count=" << response->nodes_size()
              << std::endl;

    for (const auto& info : response->nodes()) {
        std::cout << "node name=" << info.node_name()
                  << " kind=" << info.kind()
                  << " pid=" << info.process_id()
                  << " topics=" << info.topics_size()
                  << " services=" << info.services_size()
                  << std::endl;
    }

    openember::Shutdown();
    return response->status().code() == 0 ? 0 : 1;
}
