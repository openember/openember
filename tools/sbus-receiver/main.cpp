// sbus-receiver — SBUS frame monitor (LPIO SBUS)
//
// SPDX-License-Identifier: Apache-2.0

#include "lpio/SBUS.hpp"

#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <atomic>
#include <chrono>
#include <string>
#include <thread>

namespace {

std::atomic<bool> g_running {true};

void onSignal(int)
{
    g_running = false;
}

void printUsage(const char* prog)
{
    std::fprintf(stderr,
                 "Usage: %s [options] <serial_device>\n"
                 "\n"
                 "Receive Futaba SBUS frames and print channel values.\n"
                 "\n"
                 "Options:\n"
                 "  -b, --baud <rate>     Baud rate (default: 100000)\n"
                 "  -i, --interval <ms>   Print interval (default: 50)\n"
                 "  -1, --once            Print one frame and exit\n"
                 "  -h, --help            Show help\n",
                 prog);
}

struct Options {
    std::string device;
    uint32_t    baudRate      = lpio::kSbusDefaultBaudRate;
    int         printIntervalMs = 50;
    bool        once          = false;
    bool        showHelp      = false;
};

bool parseOptions(int argc, char* argv[], Options* opts)
{
    int positional = 0;

    for (int i = 1; i < argc; ++i) {
        const char* arg = argv[i];

        if (std::strcmp(arg, "-h") == 0 || std::strcmp(arg, "--help") == 0) {
            opts->showHelp = true;
            return true;
        }
        if (std::strcmp(arg, "-1") == 0 || std::strcmp(arg, "--once") == 0) {
            opts->once = true;
            continue;
        }
        if (std::strcmp(arg, "-b") == 0 || std::strcmp(arg, "--baud") == 0) {
            if (++i >= argc) {
                return false;
            }
            opts->baudRate = static_cast<uint32_t>(std::strtoul(argv[i], nullptr, 10));
            continue;
        }
        if (std::strcmp(arg, "-i") == 0 || std::strcmp(arg, "--interval") == 0) {
            if (++i >= argc) {
                return false;
            }
            opts->printIntervalMs = static_cast<int>(std::strtol(argv[i], nullptr, 10));
            continue;
        }
        if (arg[0] == '-') {
            return false;
        }
        if (positional == 0) {
            opts->device = arg;
            ++positional;
        } else {
            return false;
        }
    }

    return positional == 1;
}

void printChannelRegistry(const lpio::SBUS& sbus)
{
    std::fprintf(stderr, "Registered SBUS channels (%zu PWM + %zu digital):\n",
                 lpio::kSbusChannelCount,
                 lpio::kSbusSwitchCount);

    for (const auto& ch : sbus.channels()) {
        std::fprintf(stderr,
                     "  [%2u] %-4s %-10s  raw %4u..%4u  center %4u\n",
                     ch.index,
                     ch.name.data(),
                     ch.label.data(),
                     ch.minValue,
                     ch.maxValue,
                     ch.centerValue);
    }

    for (const auto& sw : sbus.switches()) {
        std::fprintf(stderr,
                     "  SW%u %-4s %-18s  values %u / %u\n",
                     sw.index + 1,
                     sw.name.data(),
                     sw.label.data(),
                     sw.offValue,
                     sw.onValue);
    }

    std::fprintf(stderr, "\n");
}

void printFrame(const lpio::SBUS& sbus, const lpio::SbusFrame& frame, uint64_t seq)
{
    std::printf("--- frame #%llu", static_cast<unsigned long long>(seq));
    if (frame.failsafe) {
        std::printf("  [FAILSAFE]");
    }
    if (frame.frameLost) {
        std::printf("  [LOST]");
    }
    std::printf(" ---\n");

    for (std::size_t i = 0; i < lpio::kSbusChannelCount; ++i) {
        const auto& info = sbus.channels()[i];
        const uint16_t raw = frame.channels[i];
        const float    pct = lpio::SBUS::rawToPercent(raw, info);
        const float    us  = lpio::SBUS::rawToMicroseconds(raw, info);
        const float    norm = lpio::SBUS::rawToNormalized(raw, info);

        std::printf("  %-4s %-10s  raw=%4u  us=%7.1f  pct=%6.1f%%  norm=%+.3f\n",
                    info.name.data(),
                    info.label.data(),
                    raw,
                    us,
                    pct,
                    norm);
    }

    for (std::size_t i = 0; i < lpio::kSbusSwitchCount; ++i) {
        const auto& info = sbus.switches()[i];
        std::printf("  %-4s %-18s  %u\n",
                    info.name.data(),
                    info.label.data(),
                    frame.switches[i]);
    }

    std::printf("\n");
}

}  // namespace

int main(int argc, char* argv[])
{
    Options opts;
    if (!parseOptions(argc, argv, &opts)) {
        printUsage(argv[0]);
        return 1;
    }

    if (opts.showHelp) {
        printUsage(argv[0]);
        return 0;
    }

    std::signal(SIGINT, onSignal);
    std::signal(SIGTERM, onSignal);

    try {
        auto sbus = lpio::SBUS::on(opts.device)
                        .baudRate(opts.baudRate)
                        .nonBlocking(true)
                        .buildAndOpen();

        sbus.useDefaultChannelMap();

        std::fprintf(stderr,
                     "sbus-receiver: %s @ %u baud (8E2)\n",
                     opts.device.c_str(),
                     opts.baudRate);

        printChannelRegistry(sbus);

        uint64_t seq = 0;
        while (g_running) {
            const auto frame = sbus.recvFrame(std::chrono::milliseconds(500));
            ++seq;
            printFrame(sbus, frame, seq);

            if (opts.once) {
                break;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(opts.printIntervalMs));
        }
    } catch (const std::exception& ex) {
        std::fprintf(stderr, "sbus-receiver: %s\n", ex.what());
        return 1;
    }

    return 0;
}
