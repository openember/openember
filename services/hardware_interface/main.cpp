/*
 * Copyright (c) 2026, OpenEmber Team
 * SPDX-License-Identifier: Apache-2.0
 */

#include <exception>
#include <iostream>
#include <string>
#include <utility>

#include "openember/framework/system_bus.hpp"
#include "openember/init.hpp"
#include "openember/node.hpp"
#include "openember/services/hardware_interface/hardware_config.hpp"
#include "openember/services/hardware_interface/hardware_interface_app.hpp"

namespace {

struct CliOptions {
    std::string config_path;
    bool show_help = false;
};

void PrintUsage(const char* program) {
    std::cout << "Usage: " << program << " [--config <path>]\n"
              << "\n"
              << "Options:\n"
              << "  --config <path>  Load Hardware Interface YAML config\n"
              << "  -h, --help       Show this help\n";
}

bool ParseArgs(int argc, char** argv, CliOptions* options) {
    for (int i = 1; i < argc; ++i) {
        const std::string arg(argv[i]);
        if (arg == "-h" || arg == "--help") {
            options->show_help = true;
            return true;
        }
        if (arg == "--config") {
            if (i + 1 >= argc) {
                std::cerr << "--config requires a path" << std::endl;
                return false;
            }
            options->config_path = argv[++i];
            continue;
        }
        std::cerr << "unknown argument: " << arg << std::endl;
        return false;
    }
    return true;
}

}  // namespace

int main(int argc, char** argv) {
    CliOptions cli;
    if (!ParseArgs(argc, argv, &cli)) {
        PrintUsage(argv[0]);
        return 2;
    }
    if (cli.show_help) {
        PrintUsage(argv[0]);
        return 0;
    }

    try {
        auto config = openember::services::hardware_interface::DefaultMockConfig();
        if (!cli.config_path.empty()) {
            auto loaded =
                openember::services::hardware_interface::LoadConfigFromFile(
                    cli.config_path);
            if (!loaded.Ok()) {
                std::cerr << "hardware_interface config error: "
                          << loaded.Err().message << std::endl;
                return 2;
            }
            config = std::move(loaded.Value());
            std::cout << "hardware_interface loaded config: "
                      << cli.config_path << std::endl;
        }

        openember::framework::InitSystemClient(config.robot_id);
        auto node = openember::CreateNode(config.node_name);

        openember::services::hardware_interface::HardwareInterfaceAppOptions
            app_options;
        app_options.robot_id = config.robot_id;
        app_options.node_name = config.node_name;
        app_options.instance_id = config.instance_id;

        openember::services::hardware_interface::HardwareInterfaceApp app(
            node,
            std::move(config),
            std::move(app_options));

        if (!app.Start()) {
            std::cerr << "hardware_interface failed to start" << std::endl;
            app.Stop();
            openember::Shutdown();
            return 1;
        }

        app.Spin();
        app.Stop();
        openember::Shutdown();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "hardware_interface failed: " << e.what() << std::endl;
        openember::Shutdown();
        return 1;
    }
}
