/*
 * Copyright (c) 2022-2026, OpenEmber Team
 * SPDX-License-Identifier: Apache-2.0
 *
 * OE-AFP Modbus TCP master: poll IO card / gateway virtual slave → /agri topics.
 *
 * Manual (this process owns Link router; app uses --client):
 *   ./openember_agri_io_card_mock --port 1502 --channels 10
 *   ./openember_agri_field_master --router --endpoint 127.0.0.1:1502
 *   ./openember_agri_station_app --client
 *
 * Under launch_manager the router is already up — use --client there.
 */

#include <chrono>
#include <cstdint>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include "agri_afp_regs.hpp"
#include "agri_common.hpp"
#include "agri_modbus_tcp.hpp"

#include "openember/init.hpp"
#include "openember/msgs/sensor/v1/sensor.pb.h"
#include "openember/node.hpp"

namespace {

constexpr const char* kNodeName = "agri_field_master";

struct CliOptions {
    agri_demo::LinkCliOptions link;
    std::string endpoint = "127.0.0.1:1502";
    std::string bus_id = "io0";
    std::uint8_t unit = 1;
    std::uint16_t channels = agri_demo::afp::kChannelDemoDefault;
    int poll_ms = agri_demo::afp::kDefaultPollPeriodMs;
    int timeout_ms = agri_demo::afp::kDefaultTcpTimeoutMs;
    std::uint16_t ui_channel = 0;
};

CliOptions ParseOptions(int argc, char** argv) {
    CliOptions options;
    options.link = agri_demo::ParseLinkOptions(argc, argv);
    for (int i = 1; i < argc; ++i) {
        const std::string arg(argv[i]);
        if (arg == "--endpoint" && i + 1 < argc) {
            options.endpoint = argv[++i];
        } else if (arg == "--bus-id" && i + 1 < argc) {
            options.bus_id = argv[++i];
        } else if (arg == "--unit" && i + 1 < argc) {
            options.unit = static_cast<std::uint8_t>(std::stoi(argv[++i]));
        } else if (arg == "--channels" && i + 1 < argc) {
            options.channels =
                static_cast<std::uint16_t>(std::stoi(argv[++i]));
        } else if (arg == "--poll-ms" && i + 1 < argc) {
            options.poll_ms = std::stoi(argv[++i]);
        } else if (arg == "--timeout-ms" && i + 1 < argc) {
            options.timeout_ms = std::stoi(argv[++i]);
        } else if (arg == "--ui-channel" && i + 1 < argc) {
            options.ui_channel =
                static_cast<std::uint16_t>(std::stoi(argv[++i]));
        }
    }
    if (options.channels < 1 ||
        options.channels > agri_demo::afp::kChannelMax) {
        throw std::runtime_error("channels out of range");
    }
    if (options.ui_channel >= options.channels) {
        throw std::runtime_error("ui-channel out of range");
    }
    return options;
}

struct ChannelSample {
    double temp_c = 0;
    double humidity = 0;
    double lux = 0;
    double soil = 0;
    double soil_temp = 0;
    std::uint16_t quality = 0;
    std::uint32_t seq = 0;
    bool ok = false;
};

ChannelSample DecodeChannel(const std::vector<std::uint16_t>& block) {
    using namespace agri_demo::afp;
    ChannelSample s;
    if (block.size() < kChannelStride) {
        return s;
    }
    s.temp_c = DecodeTempC(static_cast<std::int16_t>(block[kChOffAirTempC10]));
    s.humidity = DecodeHumidityPct(block[kChOffAirHumidityPct10]);
    s.lux = block[kChOffIlluminanceLux];
    s.soil = DecodeSoilMoisturePct(block[kChOffSoilMoisturePct10]);
    s.soil_temp =
        DecodeSoilTempC(static_cast<std::int16_t>(block[kChOffSoilTempC10]));
    s.quality = block[kChOffQuality];
    s.seq = UnpackSeq(block[kChOffSeqLo], block[kChOffSeqHi]);
    s.ok = (s.quality & kQualityGood) != 0 &&
           (s.quality & kQualityCommLost) == 0;
    return s;
}

}  // namespace

int main(int argc, char** argv) {
    try {
        const auto cli = ParseOptions(argc, argv);
        const auto link_mode = agri_demo::InitLink(cli.link);

        auto node = openember::CreateNode(kNodeName);
        auto temp_pub =
            node->Advertise<openember::msgs::sensor::v1::TemperatureSample>(
                agri_demo::kAirTempTopic);
        auto humidity_pub =
            node->Advertise<openember::msgs::sensor::v1::ScalarSample>(
                agri_demo::kAirHumidityTopic);
        auto light_pub =
            node->Advertise<openember::msgs::sensor::v1::ScalarSample>(
                agri_demo::kLightTopic);
        auto soil_pub =
            node->Advertise<openember::msgs::sensor::v1::ScalarSample>(
                agri_demo::kSoilMoistureTopic);

        std::string host;
        int port = 0;
        agri_demo::modbus_tcp::ParseHostPort(cli.endpoint, &host, &port);

        std::cout << kNodeName << " started as Link " << link_mode
                  << "\n  endpoint " << cli.endpoint << " unit="
                  << static_cast<int>(cli.unit)
                  << " channels=" << cli.channels << " bus=" << cli.bus_id
                  << " ui_channel=" << cli.ui_channel << '\n'
                  << std::flush;

        agri_demo::modbus_tcp::TcpClient client;
        int fail_streak = 0;
        std::uint64_t sequence = 0;
        std::uint16_t heartbeat = 1;

        while (openember::Ok()) {
            if (!client.connected()) {
                try {
                    client.Connect(host, port, cli.timeout_ms);
                    std::cout << kNodeName << " connected to " << cli.endpoint
                              << '\n'
                              << std::flush;
                    fail_streak = 0;
                } catch (const std::exception& e) {
                    std::cerr << kNodeName << " connect: " << e.what() << '\n';
                    std::this_thread::sleep_for(
                        std::chrono::milliseconds(cli.poll_ms));
                    continue;
                }
            }

            std::vector<std::uint16_t> info;
            if (!client.ReadHoldingRegisters(
                    cli.unit,
                    agri_demo::afp::kDeviceInfoBase,
                    agri_demo::afp::kDeviceInfoCount,
                    &info)) {
                ++fail_streak;
                std::cerr << kNodeName << " info poll failed streak="
                          << fail_streak << '\n';
                if (fail_streak >= agri_demo::afp::kDefaultCommFailThreshold) {
                    client.Close();
                }
                std::this_thread::sleep_for(
                    std::chrono::milliseconds(cli.poll_ms));
                continue;
            }

            // Modbus caps a single FC03 at 125 registers; chunk channel blocks.
            constexpr std::uint16_t kMaxRegsPerRead = 120;  // multiple of stride
            std::vector<std::uint16_t> channels;
            channels.reserve(agri_demo::afp::ChannelBlockCount(cli.channels));
            bool ch_ok = true;
            std::uint16_t remaining =
                agri_demo::afp::ChannelBlockCount(cli.channels);
            std::uint16_t addr = agri_demo::afp::kChannelBase;
            while (remaining > 0) {
                const std::uint16_t qty =
                    remaining > kMaxRegsPerRead ? kMaxRegsPerRead : remaining;
                std::vector<std::uint16_t> chunk;
                if (!client.ReadHoldingRegisters(cli.unit, addr, qty, &chunk)) {
                    ch_ok = false;
                    break;
                }
                channels.insert(channels.end(), chunk.begin(), chunk.end());
                addr = static_cast<std::uint16_t>(addr + qty);
                remaining = static_cast<std::uint16_t>(remaining - qty);
            }

            if (!ch_ok) {
                ++fail_streak;
                std::cerr << kNodeName << " channel poll failed streak="
                          << fail_streak << '\n';
                if (fail_streak >= agri_demo::afp::kDefaultCommFailThreshold) {
                    client.Close();
                }
                std::this_thread::sleep_for(
                    std::chrono::milliseconds(cli.poll_ms));
                continue;
            }
            fail_streak = 0;

            if (info.size() >= agri_demo::afp::kDeviceInfoCount) {
                if (info[agri_demo::afp::kRegProtoMagic] !=
                    agri_demo::afp::kProtoMagic) {
                    std::cerr << kNodeName << " bad proto magic\n";
                }
                (void)client.WriteSingleRegister(
                    cli.unit, agri_demo::afp::kRegHeartbeat, heartbeat++);
            }

            std::vector<ChannelSample> samples(cli.channels);
            for (std::uint16_t ch = 0; ch < cli.channels; ++ch) {
                const auto off = static_cast<std::size_t>(ch) *
                                 agri_demo::afp::kChannelStride;
                std::vector<std::uint16_t> block(
                    channels.begin() + static_cast<std::ptrdiff_t>(off),
                    channels.begin() +
                        static_cast<std::ptrdiff_t>(off +
                                                    agri_demo::afp::kChannelStride));
                samples[ch] = DecodeChannel(block);
            }

            const auto& ui = samples[cli.ui_channel];
            if (ui.ok) {
                const auto sid_temp = agri_demo::afp::MakeSensorId(
                    cli.bus_id,
                    cli.unit,
                    cli.ui_channel,
                    agri_demo::afp::kQtyAirTemperature);
                const auto sid_hum = agri_demo::afp::MakeSensorId(
                    cli.bus_id,
                    cli.unit,
                    cli.ui_channel,
                    agri_demo::afp::kQtyAirHumidity);
                const auto sid_lux = agri_demo::afp::MakeSensorId(
                    cli.bus_id,
                    cli.unit,
                    cli.ui_channel,
                    agri_demo::afp::kQtyIlluminance);
                const auto sid_soil = agri_demo::afp::MakeSensorId(
                    cli.bus_id,
                    cli.unit,
                    cli.ui_channel,
                    agri_demo::afp::kQtySoilMoisture);

                (void)temp_pub.Publish(agri_demo::MakeTemperature(
                    kNodeName, sid_temp, sequence, ui.temp_c));
                (void)humidity_pub.Publish(agri_demo::MakeScalar(
                    kNodeName,
                    sid_hum,
                    sequence,
                    "relative_humidity",
                    ui.humidity,
                    "%"));
                (void)light_pub.Publish(agri_demo::MakeScalar(
                    kNodeName, sid_lux, sequence, "illuminance", ui.lux, "lux"));
                (void)soil_pub.Publish(agri_demo::MakeScalar(
                    kNodeName,
                    sid_soil,
                    sequence,
                    "soil_moisture",
                    ui.soil,
                    "%"));
            }

            if (sequence % 4 == 0) {
                std::cout << kNodeName << " poll#" << sequence;
                for (std::uint16_t ch = 0; ch < cli.channels; ++ch) {
                    const auto& s = samples[ch];
                    std::cout << " ch" << ch << "={T=" << s.temp_c
                              << " H=" << s.humidity << " soil=" << s.soil
                              << " q=0x" << std::hex << s.quality << std::dec
                              << '}';
                }
                std::cout << '\n' << std::flush;
            }

            ++sequence;
            std::this_thread::sleep_for(std::chrono::milliseconds(cli.poll_ms));
        }

        openember::Shutdown();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "agri_field_master failed: " << e.what() << std::endl;
        openember::Shutdown();
        return 1;
    }
}
