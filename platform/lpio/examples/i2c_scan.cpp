#include "lpio/I2CBus.hpp"

#include <cstdio>
#include <cstdlib>
#include <span>

int main(int argc, char* argv[])
{
    if (argc < 2) {
        std::fprintf(stderr, "usage: %s <i2c_bus_number>\n", argv[0]);
        return 1;
    }

    const uint32_t busNum = static_cast<uint32_t>(std::strtoul(argv[1], nullptr, 10));
    int found             = 0;

    std::printf("Scanning I2C bus %u...\n", busNum);

    for (uint8_t addr = 0x03; addr < 0x78; ++addr) {
        try {
            auto bus = lpio::I2CBus::create().bus(busNum).address(addr).buildAndOpen();

            uint8_t dummy = 0;
            bus.read(std::span<std::byte>(reinterpret_cast<std::byte*>(&dummy), 1));

            std::printf("  0x%02X\n", addr);
            ++found;
            bus.close();
        } catch (...) {
            // no device at this address
        }
    }

    std::printf("Done. %d device(s) found.\n", found);
    return 0;
}
