#include <iostream>

#include "openember/init.hpp"
#include "openember/link/options.hpp"
#include "openember/msgs/runtime/v1/runtime.pb.h"
#include "openember/node.hpp"

namespace {

constexpr const char* kRuntimeProcessEventsTopic = "/runtime/process/events";

}  // namespace

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    openember::RuntimeOptions options;
    options.robot_id = "openember";
    options.link = openember::link::LocalClientOptions();

    openember::Init(options);

    auto node = openember::CreateNode("runtime_process_listener");
    auto sub = node->Subscribe<openember::msgs::runtime::v1::ProcessEvent>(
        kRuntimeProcessEventsTopic,
        [](const openember::msgs::runtime::v1::ProcessEvent& event) {
            const auto& process = event.process();
            std::cout << "recv ProcessEvent"
                      << " sequence=" << event.header().sequence()
                      << " name=" << process.name()
                      << " pid=" << process.process_id()
                      << " state=" << process.state()
                      << " exit_code=" << process.exit_code()
                      << " message=\"" << process.message() << "\""
                      << std::endl;
        });

    std::cout << "listening for runtime process events on "
              << kRuntimeProcessEventsTopic
              << "... press Enter to exit"
              << std::endl;
    std::cin.get();

    openember::Shutdown();
    return 0;
}
