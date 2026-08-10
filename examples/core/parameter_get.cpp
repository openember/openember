#include <chrono>
#include <cstdint>
#include <iostream>
#include <string>

#include "openember/init.hpp"
#include "openember/link/options.hpp"
#include "openember/msgs/parameter/v1/parameter.pb.h"
#include "openember/node.hpp"

namespace {

constexpr const char* kRobotId = "openember";
constexpr const char* kNodeName = "parameter_get";
constexpr const char* kGetParameterService = "/parameters/get";

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
    header->set_request_id("parameter-get");
}

std::string ValueToString(
    const openember::msgs::parameter::v1::ParameterValue& value) {
    switch (value.value_case()) {
        case openember::msgs::parameter::v1::ParameterValue::kBoolValue:
            return value.bool_value() ? "true" : "false";
        case openember::msgs::parameter::v1::ParameterValue::kInt64Value:
            return std::to_string(value.int64_value());
        case openember::msgs::parameter::v1::ParameterValue::kUint64Value:
            return std::to_string(value.uint64_value());
        case openember::msgs::parameter::v1::ParameterValue::kDoubleValue:
            return std::to_string(value.double_value());
        case openember::msgs::parameter::v1::ParameterValue::kStringValue:
            return value.string_value();
        case openember::msgs::parameter::v1::ParameterValue::kBytesValue:
            return "<bytes>";
        case openember::msgs::parameter::v1::ParameterValue::VALUE_NOT_SET:
            break;
    }
    return "<unset>";
}

}  // namespace

int main(int argc, char** argv) {
    openember::RuntimeOptions options;
    options.robot_id = kRobotId;
    options.link = openember::link::LocalClientOptions();
    openember::Init(options);

    auto node = openember::CreateNode(kNodeName);
    auto client = node->CreateClient<
        openember::msgs::parameter::v1::GetParameterRequest,
        openember::msgs::parameter::v1::GetParameterResponse>(kGetParameterService);

    openember::msgs::parameter::v1::GetParameterRequest request;
    FillHeader(request.mutable_header());
    request.set_node_name("config_service");
    for (int i = 1; i < argc; ++i) {
        request.add_names(argv[i]);
    }

    const auto response = client.Call(request, std::chrono::seconds(3));
    if (!response.has_value()) {
        std::cerr << "parameter get failed or timed out" << std::endl;
        openember::Shutdown();
        return 1;
    }

    std::cout << "parameter get status=" << response->status().code()
              << " message=\"" << response->status().message() << "\""
              << " count=" << response->parameters_size()
              << std::endl;

    for (const auto& parameter : response->parameters()) {
        std::cout << parameter.definition().name()
                  << "="
                  << ValueToString(parameter.value())
                  << " type="
                  << parameter.definition().type()
                  << " read_only="
                  << parameter.definition().read_only()
                  << std::endl;
    }

    openember::Shutdown();
    return response->status().code() == 0 ? 0 : 1;
}
