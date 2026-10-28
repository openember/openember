/*
 * Copyright (c) 2022-2026, OpenEmber Team
 * SPDX-License-Identifier: Apache-2.0
 *
 * Mock agri sensors for host bring-up.
 *   --role climate  air temperature / humidity / illuminance
 *   --role soil     soil moisture
 *   --role all      both (default for single-process debug)
 *
 * Launch uses separate climate and soil processes so one can be killed
 * to exercise degrade behavior in agri_station_app.
 */

#include <chrono>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <string>
#include <thread>

#include "agri_common.hpp"

#include "openember/init.hpp"
#include "openember/msgs/sensor/v1/sensor.pb.h"
#include "openember/node.hpp"

namespace {

constexpr const char* kNodeClimate = "agri_mock_climate";
constexpr const char* kNodeSoil = "agri_mock_soil";
constexpr const char* kNodeAll = "agri_mock_sensors";

enum class Role {
    kClimate,
    kSoil,
    kAll,
};

struct CliOptions {
    agri_demo::LinkCliOptions link;
    Role role = Role::kAll;
};

CliOptions ParseOptions(int argc, char** argv) {
    CliOptions options;
    options.link = agri_demo::ParseLinkOptions(argc, argv);
    for (int i = 1; i < argc; ++i) {
        const std::string arg(argv[i]);
        if (arg == "--role" && i + 1 < argc) {
            const std::string role(argv[++i]);
            if (role == "climate") {
                options.role = Role::kClimate;
            } else if (role == "soil") {
                options.role = Role::kSoil;
            } else if (role == "all") {
                options.role = Role::kAll;
            }
        }
    }
    return options;
}

const char* NodeNameForRole(Role role) {
    switch (role) {
        case Role::kClimate:
            return kNodeClimate;
        case Role::kSoil:
            return kNodeSoil;
        case Role::kAll:
            return kNodeAll;
    }
    return kNodeAll;
}

}  // namespace

int main(int argc, char** argv) {
    try {
        const auto cli = ParseOptions(argc, argv);
        const auto link_mode = agri_demo::InitLink(cli.link);
        const char* node_name = NodeNameForRole(cli.role);

        auto node = openember::CreateNode(node_name);

        openember::Publisher<openember::msgs::sensor::v1::TemperatureSample> temp_pub;
        openember::Publisher<openember::msgs::sensor::v1::ScalarSample> humidity_pub;
        openember::Publisher<openember::msgs::sensor::v1::ScalarSample> light_pub;
        openember::Publisher<openember::msgs::sensor::v1::ScalarSample> soil_pub;

        const bool do_climate =
            cli.role == Role::kClimate || cli.role == Role::kAll;
        const bool do_soil = cli.role == Role::kSoil || cli.role == Role::kAll;

        if (do_climate) {
            temp_pub = node->Advertise<openember::msgs::sensor::v1::TemperatureSample>(
                agri_demo::kAirTempTopic);
            humidity_pub = node->Advertise<openember::msgs::sensor::v1::ScalarSample>(
                agri_demo::kAirHumidityTopic);
            light_pub = node->Advertise<openember::msgs::sensor::v1::ScalarSample>(
                agri_demo::kLightTopic);
        }
        if (do_soil) {
            soil_pub = node->Advertise<openember::msgs::sensor::v1::ScalarSample>(
                agri_demo::kSoilMoistureTopic);
        }

        std::cout << node_name << " started as Link " << link_mode << " role=";
        switch (cli.role) {
            case Role::kClimate:
                std::cout << "climate";
                break;
            case Role::kSoil:
                std::cout << "soil";
                break;
            case Role::kAll:
                std::cout << "all";
                break;
        }
        std::cout << '\n' << std::flush;

        std::uint64_t sequence = 0;
        const auto start = std::chrono::steady_clock::now();

        while (openember::Ok()) {
            const auto elapsed =
                std::chrono::duration<double>(std::chrono::steady_clock::now() - start)
                    .count();

            if (do_climate) {
                const double temp_c = 24.0 + 2.0 * std::sin(elapsed / 30.0);
                const double humidity = 55.0 + 8.0 * std::sin(elapsed / 40.0);
                const double lux = 12000.0 + 4000.0 * std::sin(elapsed / 20.0);
                (void)temp_pub.Publish(agri_demo::MakeTemperature(
                    node_name, "aht20", sequence, temp_c));
                (void)humidity_pub.Publish(agri_demo::MakeScalar(
                    node_name, "aht20", sequence, "relative_humidity", humidity, "%"));
                (void)light_pub.Publish(agri_demo::MakeScalar(
                    node_name, "bh1750", sequence, "illuminance", lux, "lux"));
            }

            if (do_soil) {
                // Slowly dry out then recover — triggers app alerts.
                const double moisture = 35.0 + 25.0 * std::sin(elapsed / 45.0);
                (void)soil_pub.Publish(agri_demo::MakeScalar(
                    node_name,
                    "soil_probe",
                    sequence,
                    "soil_moisture",
                    moisture,
                    "%"));
            }

            if (sequence % 5 == 0) {
                std::cout << node_name << " tick sequence=" << sequence << '\n'
                          << std::flush;
            }
            ++sequence;
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }

        openember::Shutdown();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "agri_mock_sensors failed: " << e.what() << std::endl;
        return 1;
    }
}
