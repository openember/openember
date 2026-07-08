#include "lpio/CANBus.hpp"

#include <chrono>
#include <cstdio>
#include <csignal>
#include <atomic>
#include <exception>

namespace {

std::atomic<bool> g_running {true};

void onSignal(int)
{
    g_running = false;
}

}  // namespace

int main(int argc, char* argv[])
{
    if (argc < 2) {
        std::fprintf(stderr, "usage: %s <can_ifname>\n", argv[0]);
        return 1;
    }

    std::signal(SIGINT, onSignal);
    std::signal(SIGTERM, onSignal);

    try {
        auto can = lpio::CANBus::on(argv[1]).open();
        std::printf("Listening on %s (Ctrl+C to quit)\n", argv[1]);

        while (g_running) {
            try {
                const lpio::CANFrame frame =
                    can.recvFrame(std::chrono::milliseconds(500));

                std::printf("id=0x%03X len=%u ext=%d fd=%d data=",
                            frame.canId,
                            frame.dataLen,
                            frame.isExtended ? 1 : 0,
                            frame.isFd ? 1 : 0);

                for (uint8_t i = 0; i < frame.dataLen; ++i) {
                    std::printf("%s%02X", i ? " " : "", frame.data[i]);
                }
                std::printf("\n");
            } catch (const lpio::DeviceTimeoutError&) {
                // no frame within timeout
            }
        }
    } catch (const std::exception& ex) {
        std::fprintf(stderr, "error: %s\n", ex.what());
        return 1;
    }

    return 0;
}
