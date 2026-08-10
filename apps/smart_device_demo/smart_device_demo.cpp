#include <chrono>
#include <cstdint>
#include <exception>
#include <iostream>
#include <string>
#include <thread>

#include <unistd.h>

#include "openember/init.hpp"
#include "openember/link/options.hpp"
#include "openember/msgs/common/v1/common.pb.h"
#include "openember/msgs/device/v1/device.pb.h"
#include "openember/msgs/lifecycle/v1/lifecycle.pb.h"
#include "openember/msgs/node/v1/node.pb.h"
#include "openember/msgs/parameter/v1/parameter.pb.h"
#include "openember/node.hpp"

namespace {

constexpr const char* kRobotId = "openember";
constexpr const char* kInstanceId = "demo-instance";
constexpr const char* kNodeName = "smart_device_app";
constexpr const char* kNodeVersion = "0.1.0";
constexpr const char* kNodeInfoTopic = "/nodes/smart_device_app/info";
constexpr const char* kHeartbeatTopic = "/nodes/smart_device_app/heartbeat";
constexpr const char* kGetParameterService = "/parameters/get";
constexpr const char* kDeviceQueryService = "/devices/query";
constexpr std::uint64_t kNodeInfoPublishInterval = 10;

enum class LinkMode {
    kAuto,
    kRouter,
    kClient,
};

enum class ProductState {
    kBooting,
    kInitializing,
    kReady,
    kRunning,
};

struct LinkCliOptions {
    LinkMode mode = LinkMode::kAuto;
    std::string connect = "tcp/127.0.0.1:7447";
    std::string listen = "tcp/127.0.0.1:7447";
};

std::uint64_t UnixTimeNs() {
    const auto now = std::chrono::system_clock::now().time_since_epoch();
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(now).count());
}

void FillHeader(openember::msgs::common::v1::Header* header,
                const std::string& node_name,
                std::uint64_t sequence) {
    header->set_source_node(node_name);
    header->set_source_instance(kInstanceId);
    header->set_robot_id(kRobotId);
    header->set_sequence(sequence);
    header->set_timestamp_unix_ns(UnixTimeNs());
}

void ConfigureQos(openember::msgs::common::v1::QosProfile* qos) {
    qos->set_queue_size(8);
    qos->set_reliability(openember::msgs::common::v1::RELIABILITY_RELIABLE);
    qos->set_durability(openember::msgs::common::v1::DURABILITY_VOLATILE);
}

void AddTopic(openember::msgs::node::v1::NodeInfo* info,
              const std::string& name,
              const std::string& type,
              openember::msgs::node::v1::EndpointDirection direction) {
    auto* topic = info->add_topics();
    topic->set_name(name);
    topic->set_type(type);
    topic->set_direction(direction);
    ConfigureQos(topic->mutable_qos());
}

void AddService(openember::msgs::node::v1::NodeInfo* info,
                const std::string& name,
                const std::string& request_type,
                const std::string& response_type) {
    auto* service = info->add_services();
    service->set_name(name);
    service->set_request_type(request_type);
    service->set_response_type(response_type);
    service->set_direction(openember::msgs::node::v1::ENDPOINT_DIRECTION_SERVICE_CLIENT);
    ConfigureQos(service->mutable_qos());
}

void AddLabel(openember::msgs::node::v1::NodeInfo* info,
              const std::string& key,
              const std::string& value) {
    auto* label = info->add_labels();
    label->set_key(key);
    label->set_value(value);
}

std::string ProductStateName(ProductState state) {
    switch (state) {
        case ProductState::kBooting:
            return "Booting";
        case ProductState::kInitializing:
            return "Initializing";
        case ProductState::kReady:
            return "Ready";
        case ProductState::kRunning:
            return "Running";
    }
    return "Unknown";
}

double ProductStateMetricValue(ProductState state) {
    switch (state) {
        case ProductState::kBooting:
            return 0.0;
        case ProductState::kInitializing:
            return 1.0;
        case ProductState::kReady:
            return 2.0;
        case ProductState::kRunning:
            return 3.0;
    }
    return -1.0;
}

ProductState UpdateProductState(std::uint64_t uptime_ms,
                                bool config_ready,
                                bool devices_ready) {
    if (uptime_ms < 1000) {
        return ProductState::kBooting;
    }
    if (!config_ready || !devices_ready) {
        return ProductState::kInitializing;
    }
    if (uptime_ms < 5000) {
        return ProductState::kReady;
    }
    return ProductState::kRunning;
}

openember::msgs::node::v1::NodeInfo BuildNodeInfo(const std::string& node_name,
                                                  std::uint64_t sequence,
                                                  std::uint64_t start_time_unix_ns) {
    openember::msgs::node::v1::NodeInfo info;
    FillHeader(info.mutable_header(), node_name, sequence);
    info.set_node_name(node_name);
    info.set_instance_id(kInstanceId);
    info.set_process_name(kNodeName);
    info.set_process_id(static_cast<std::uint32_t>(getpid()));
    info.set_kind(openember::msgs::node::v1::NODE_KIND_APPLICATION);
    info.set_version(kNodeVersion);
    info.set_start_time_unix_ns(start_time_unix_ns);

    AddTopic(&info,
             kNodeInfoTopic,
             "openember.msgs.node.v1.NodeInfo",
             openember::msgs::node::v1::ENDPOINT_DIRECTION_PUBLISHER);
    AddTopic(&info,
             kHeartbeatTopic,
             "openember.msgs.node.v1.NodeHeartbeat",
             openember::msgs::node::v1::ENDPOINT_DIRECTION_PUBLISHER);
    AddService(&info,
               kGetParameterService,
               "openember.msgs.parameter.v1.GetParameterRequest",
               "openember.msgs.parameter.v1.GetParameterResponse");
    AddService(&info,
               kDeviceQueryService,
               "openember.msgs.device.v1.DeviceQuery",
               "openember.msgs.device.v1.DeviceQueryResponse");

    AddLabel(&info, "app", "smart_device_demo");
    AddLabel(&info, "role", "product_app");
    info.add_capabilities("product_state_demo");
    info.add_capabilities("lightweight_fsm");
    info.add_capabilities("node_heartbeat");
    info.add_capabilities("config_client");
    info.add_capabilities("device_client");
    return info;
}

bool TryLoadProductMode(
    openember::Client<openember::msgs::parameter::v1::GetParameterRequest,
                      openember::msgs::parameter::v1::GetParameterResponse>& client,
    std::string* product_mode) {
    openember::msgs::parameter::v1::GetParameterRequest request;
    FillHeader(request.mutable_header(), kNodeName, 0);
    request.set_node_name("config_service");
    request.add_names("product.mode");

    const auto response = client.Call(request, std::chrono::milliseconds(500));
    if (!response.has_value() || response->status().code() != 0 ||
        response->parameters_size() == 0) {
        return false;
    }

    const auto& value = response->parameters(0).value();
    if (value.has_string_value()) {
        *product_mode = value.string_value();
    }
    return true;
}

bool TryQueryDevices(
    openember::Client<openember::msgs::device::v1::DeviceQuery,
                      openember::msgs::device::v1::DeviceQueryResponse>& client,
    int* device_count) {
    openember::msgs::device::v1::DeviceQuery request;
    FillHeader(request.mutable_header(), kNodeName, 0);
    request.set_include_state(true);

    const auto response = client.Call(request, std::chrono::milliseconds(500));
    if (!response.has_value() || response->status().code() != 0) {
        return false;
    }

    *device_count = response->devices_size();
    return response->devices_size() > 0;
}

LinkCliOptions ParseLinkOptions(int argc, char** argv) {
    LinkCliOptions options;
    for (int i = 1; i < argc; ++i) {
        const std::string arg(argv[i]);
        if (arg == "--router") {
            options.mode = LinkMode::kRouter;
        } else if (arg == "--client") {
            options.mode = LinkMode::kClient;
        } else if (arg == "--connect" && i + 1 < argc) {
            options.connect = argv[++i];
        } else if (arg == "--listen" && i + 1 < argc) {
            options.listen = argv[++i];
        }
    }
    return options;
}

void InitRouter(const LinkCliOptions& cli) {
    openember::RuntimeOptions options;
    options.robot_id = kRobotId;
    options.link.profile = openember::link::Profile::kRouter;
    options.link.listen = cli.listen;
    openember::Init(options);
}

void InitClient(const LinkCliOptions& cli) {
    openember::RuntimeOptions options;
    options.robot_id = kRobotId;
    options.link.profile = openember::link::Profile::kClient;
    options.link.connect = cli.connect;
    openember::Init(options);
}

std::string InitLink(const LinkCliOptions& cli) {
    if (cli.mode == LinkMode::kRouter) {
        InitRouter(cli);
        return "router";
    }

    if (cli.mode == LinkMode::kClient) {
        InitClient(cli);
        return "client";
    }

    try {
        InitRouter(cli);
        return "router";
    } catch (const std::exception&) {
        openember::Shutdown();
        InitClient(cli);
        return "client";
    }
}

}  // namespace

int main(int argc, char** argv) {
    try {
        const auto link_mode = InitLink(ParseLinkOptions(argc, argv));

        auto node = openember::CreateNode(kNodeName);
        auto info_pub =
            node->Advertise<openember::msgs::node::v1::NodeInfo>(
                kNodeInfoTopic);
        auto heartbeat_pub =
            node->Advertise<openember::msgs::node::v1::NodeHeartbeat>(
                kHeartbeatTopic);
        auto parameter_client = node->CreateClient<
            openember::msgs::parameter::v1::GetParameterRequest,
            openember::msgs::parameter::v1::GetParameterResponse>(
            kGetParameterService);
        auto device_client = node->CreateClient<
            openember::msgs::device::v1::DeviceQuery,
            openember::msgs::device::v1::DeviceQueryResponse>(
            kDeviceQueryService);

        const auto start = std::chrono::steady_clock::now();
        const auto start_time_unix_ns = UnixTimeNs();
        std::uint64_t info_sequence = 0;
        std::uint64_t heartbeat_sequence = 0;
        bool config_ready = false;
        bool devices_ready = false;
        std::string product_mode = "unknown";
        int device_count = 0;

        std::cout << node->Name()
                  << " started as Link "
                  << link_mode
                  << ", publishing NodeInfo on "
                  << kNodeInfoTopic
                  << " and NodeHeartbeat on "
                  << kHeartbeatTopic
                  << std::endl;

        while (openember::Ok()) {
            if (heartbeat_sequence % kNodeInfoPublishInterval == 0) {
                auto info =
                    BuildNodeInfo(node->Name(), info_sequence, start_time_unix_ns);
                if (info_pub.Publish(info)) {
                    std::cout << node->Name()
                              << " publish NodeInfo sequence="
                              << info_sequence
                              << std::endl;
                } else {
                    std::cerr << node->Name()
                              << " publish NodeInfo failed"
                              << std::endl;
                }
                ++info_sequence;
            }

            if (!config_ready) {
                config_ready = TryLoadProductMode(parameter_client, &product_mode);
                if (config_ready) {
                    std::cout << node->Name()
                              << " loaded product.mode="
                              << product_mode
                              << std::endl;
                }
            }

            if (!devices_ready) {
                devices_ready = TryQueryDevices(device_client, &device_count);
                if (devices_ready) {
                    std::cout << node->Name()
                              << " discovered devices count="
                              << device_count
                              << std::endl;
                }
            }

            openember::msgs::node::v1::NodeHeartbeat heartbeat;

            FillHeader(heartbeat.mutable_header(), node->Name(), heartbeat_sequence);

            const auto uptime = std::chrono::steady_clock::now() - start;
            const auto uptime_ms = static_cast<std::uint64_t>(
                std::chrono::duration_cast<std::chrono::milliseconds>(uptime).count());
            const auto product_state =
                UpdateProductState(uptime_ms, config_ready, devices_ready);
            heartbeat.set_node_name(node->Name());
            heartbeat.set_instance_id(kInstanceId);
            heartbeat.set_lifecycle_state(
                openember::msgs::lifecycle::v1::LIFECYCLE_STATE_ACTIVE);
            heartbeat.set_health(openember::msgs::common::v1::HEALTH_STATE_OK);
            heartbeat.set_uptime_ms(uptime_ms);
            heartbeat.set_error_count(0);
            heartbeat.set_status_message(
                "product_state=" + ProductStateName(product_state) +
                ", product_mode=" + product_mode);

            auto* uptime_metric = heartbeat.add_metrics();
            uptime_metric->set_name("app.uptime");
            uptime_metric->set_value(static_cast<double>(heartbeat.uptime_ms()));
            uptime_metric->set_unit("ms");

            auto* sequence_metric = heartbeat.add_metrics();
            sequence_metric->set_name("app.sequence");
            sequence_metric->set_value(static_cast<double>(heartbeat_sequence));
            sequence_metric->set_unit("count");

            auto* state_metric = heartbeat.add_metrics();
            state_metric->set_name("product.state");
            state_metric->set_value(ProductStateMetricValue(product_state));
            state_metric->set_unit("enum");

            auto* device_metric = heartbeat.add_metrics();
            device_metric->set_name("product.devices");
            device_metric->set_value(static_cast<double>(device_count));
            device_metric->set_unit("count");

            if (heartbeat_pub.Publish(heartbeat)) {
                std::cout << node->Name()
                          << " publish NodeHeartbeat sequence="
                          << heartbeat_sequence
                          << " state="
                          << ProductStateName(product_state)
                          << std::endl;
            } else {
                std::cerr << node->Name()
                          << " publish NodeHeartbeat failed"
                          << std::endl;
            }

            ++heartbeat_sequence;
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }

        openember::Shutdown();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "smart_device_demo failed: " << e.what() << std::endl;
        openember::Shutdown();
        return 1;
    }
}
