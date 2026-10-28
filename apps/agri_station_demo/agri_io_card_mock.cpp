/*
 * Copyright (c) 2022-2026, OpenEmber Team
 * SPDX-License-Identifier: Apache-2.0
 *
 * OE-AFP Modbus TCP slave mock of a multi-channel IO acquisition card.
 *
 *   ./openember_agri_io_card_mock [--bind 0.0.0.0] [--port 1502]
 *       [--unit 1] [--channels 10]
 */

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include "agri_afp_regs.hpp"
#include "agri_modbus_tcp.hpp"

namespace {

struct CliOptions {
    std::string bind = "0.0.0.0";
    int port = agri_demo::afp::kDefaultTcpMockPort;
    std::uint8_t unit = 1;
    std::uint16_t channels = agri_demo::afp::kChannelDemoDefault;
};

CliOptions ParseOptions(int argc, char** argv) {
    CliOptions options;
    for (int i = 1; i < argc; ++i) {
        const std::string arg(argv[i]);
        if (arg == "--bind" && i + 1 < argc) {
            options.bind = argv[++i];
        } else if (arg == "--port" && i + 1 < argc) {
            options.port = std::stoi(argv[++i]);
        } else if (arg == "--unit" && i + 1 < argc) {
            options.unit = static_cast<std::uint8_t>(std::stoi(argv[++i]));
        } else if (arg == "--channels" && i + 1 < argc) {
            options.channels =
                static_cast<std::uint16_t>(std::stoi(argv[++i]));
        } else if (arg == "--help" || arg == "-h") {
            std::cout
                << "Usage: openember_agri_io_card_mock [--bind HOST] [--port N] "
                   "[--unit N] [--channels N]\n";
            std::exit(0);
        }
    }
    if (options.channels < 1 ||
        options.channels > agri_demo::afp::kChannelMax) {
        throw std::runtime_error("channels out of range");
    }
    return options;
}

std::size_t BankSize(std::uint16_t channels) {
    return static_cast<std::size_t>(agri_demo::afp::ChannelBase(channels));
}

void WriteDeviceInfo(std::vector<std::uint16_t>& regs,
                     std::uint16_t channels,
                     std::uint32_t uptime_s) {
    using namespace agri_demo::afp;
    regs[kRegProtoMagic] = kProtoMagic;
    regs[kRegProtoVersion] = kProtoVersion;
    regs[kRegDeviceClass] = static_cast<std::uint16_t>(DeviceClass::kIoCard);
    regs[kRegChannelCount] = channels;
    regs[kRegCapsBits] = kCapDemoDefault;
    regs[kRegStatusBits] = 0;
    std::uint16_t lo = 0;
    std::uint16_t hi = 0;
    PackUptimeS(uptime_s, &lo, &hi);
    regs[kRegUptimeSLo] = lo;
    regs[kRegUptimeSHi] = hi;
    // heartbeat / sample_period_ms are R/W — leave master-written values.
    if (regs[kRegSamplePeriodMs] == 0) {
        regs[kRegSamplePeriodMs] =
            static_cast<std::uint16_t>(kDefaultPollPeriodMs);
    }
    regs[kRegLastError] = static_cast<std::uint16_t>(LastError::kOk);
}

void WriteChannelSample(std::vector<std::uint16_t>& regs,
                        std::uint16_t channel,
                        double elapsed,
                        std::uint32_t seq) {
    using namespace agri_demo::afp;
    const double phase = elapsed + channel * 0.7;
    const double temp_c = 22.0 + channel * 0.3 + 2.0 * std::sin(phase / 30.0);
    const double humidity = 50.0 + channel + 8.0 * std::sin(phase / 40.0);
    const double lux = 8000.0 + channel * 200.0 + 3000.0 * std::sin(phase / 20.0);
    // Channel 0 slowly crosses the soil-dry threshold for alert demos.
    const double moisture =
        (channel == 0) ? (35.0 + 25.0 * std::sin(elapsed / 45.0))
                       : (40.0 + channel + 10.0 * std::sin(phase / 50.0));
    const double soil_t = 18.0 + 0.5 * channel + std::sin(phase / 60.0);

    const std::uint16_t base = ChannelBase(channel);
    regs[base + kChOffAirTempC10] =
        static_cast<std::uint16_t>(EncodeTempC10(temp_c));
    regs[base + kChOffAirHumidityPct10] = EncodeHumidityPct10(humidity);
    regs[base + kChOffIlluminanceLux] =
        static_cast<std::uint16_t>(std::min(65535.0, std::max(0.0, lux)));
    regs[base + kChOffSoilMoisturePct10] = EncodeSoilMoisturePct10(moisture);
    regs[base + kChOffSoilTempC10] =
        static_cast<std::uint16_t>(EncodeSoilTempC10(soil_t));
    regs[base + kChOffCo2Ppm] = 0;
    regs[base + kChOffQuality] = kQualityGood;
    std::uint16_t slo = 0;
    std::uint16_t shi = 0;
    PackSeq(seq, &slo, &shi);
    regs[base + kChOffSeqLo] = slo;
    regs[base + kChOffSeqHi] = shi;
    regs[base + kChOffAgeMs] = 0;
}

}  // namespace

int main(int argc, char** argv) {
    try {
        const auto cli = ParseOptions(argc, argv);
        agri_demo::modbus_tcp::RegisterBank bank(BankSize(cli.channels));
        bank.WithLock([&](std::vector<std::uint16_t>& regs) {
            WriteDeviceInfo(regs, cli.channels, 0);
            for (std::uint16_t ch = 0; ch < cli.channels; ++ch) {
                WriteChannelSample(regs, ch, 0.0, 0);
            }
        });

        agri_demo::modbus_tcp::TcpServer server;
        server.Listen(cli.bind, cli.port);
        std::cout << "agri_io_card_mock listening on " << cli.bind << ':'
                  << cli.port << " unit=" << static_cast<int>(cli.unit)
                  << " channels=" << cli.channels
                  << " proto=" << agri_demo::afp::kProtoId << '\n'
                  << std::flush;

        std::atomic<bool> run{true};
        const auto start = std::chrono::steady_clock::now();
        std::thread sampler([&]() {
            std::uint32_t seq = 0;
            while (run.load()) {
                const auto elapsed =
                    std::chrono::duration<double>(
                        std::chrono::steady_clock::now() - start)
                        .count();
                const auto uptime_s = static_cast<std::uint32_t>(elapsed);
                bank.WithLock([&](std::vector<std::uint16_t>& regs) {
                    WriteDeviceInfo(regs, cli.channels, uptime_s);
                    for (std::uint16_t ch = 0; ch < cli.channels; ++ch) {
                        WriteChannelSample(regs, ch, elapsed, seq);
                    }
                });
                ++seq;
                std::this_thread::sleep_for(std::chrono::milliseconds(200));
            }
        });

        while (run.load()) {
            const int client = server.Accept(500);
            if (client < 0) {
                continue;
            }
            // Note: unit id in MBAP is echoed; demo does not filter by --unit.
            (void)cli.unit;
            while (run.load()) {
                if (!agri_demo::modbus_tcp::ServeOneRequest(client, &bank)) {
                    break;
                }
            }
            agri_demo::modbus_tcp::CloseFd(client);
        }

        run = false;
        sampler.join();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "agri_io_card_mock failed: " << e.what() << std::endl;
        return 1;
    }
}
