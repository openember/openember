#include "lpio/SerialPort.hpp"

#include <cstdio>
#include <cstdlib>
#include <exception>
#include <vector>

int main(int argc, char* argv[])
{
    if (argc < 2) {
        std::fprintf(stderr, "usage: %s <serial_device> [baud]\n", argv[0]);
        return 1;
    }

    uint32_t baud = 115200;
    if (argc >= 3) {
        baud = static_cast<uint32_t>(std::strtoul(argv[2], nullptr, 10));
    }

    try {
        auto port = lpio::SerialPort::on(argv[1]).baudRate(baud).open();

        std::vector<std::byte> buf(512);
        std::fprintf(stderr, "echo on %s @ %u baud (Ctrl+C to quit)\n", argv[1], baud);

        while (true) {
            const std::size_t n = port.read(buf);
            if (n == 0) {
                continue;
            }
            port.write(std::span<const std::byte>(buf.data(), n));
        }
    } catch (const std::exception& ex) {
        std::fprintf(stderr, "error: %s\n", ex.what());
        return 1;
    }
}
