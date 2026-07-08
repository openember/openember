// emcom — picocom-like interactive serial console (LPIO SerialPort)
//
// SPDX-License-Identifier: Apache-2.0

#include "lpio/SerialPort.hpp"

#include <termios.h>
#include <unistd.h>
#include <poll.h>

#include <atomic>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

namespace {

struct Options {
    std::string           device;
    uint32_t              baudRate   = 115200;
    uint8_t               dataBits   = 8;
    uint8_t               stopBits   = 1;
    lpio::SerialParity    parity     = lpio::SerialParity::None;
    bool                  localEcho  = false;
    char                  escapeChar = '\x01';  // Ctrl-A (picocom default)
    std::optional<std::string> sendFile;
    bool                  showHelp   = false;
};

class StdinRawMode {
public:
    bool enter()
    {
        if (!isatty(STDIN_FILENO)) {
            return false;
        }
        if (tcgetattr(STDIN_FILENO, &saved_) != 0) {
            return false;
        }

        termios raw = saved_;
        cfmakeraw(&raw);
        raw.c_lflag &= ~static_cast<tcflag_t>(ECHO);
        raw.c_cc[VMIN]  = 1;
        raw.c_cc[VTIME] = 0;

        if (tcsetattr(STDIN_FILENO, TCSANOW, &raw) != 0) {
            return false;
        }
        active_ = true;
        return true;
    }

    void leave()
    {
        if (active_) {
            tcsetattr(STDIN_FILENO, TCSANOW, &saved_);
            active_ = false;
        }
    }

    ~StdinRawMode()
    {
        leave();
    }

private:
    termios saved_ {};
    bool    active_ = false;
};

void printUsage(const char* prog)
{
    std::fprintf(stderr,
                 "Usage: %s [options] <device>\n"
                 "\n"
                 "Interactive serial console (picocom-style). Default exit: Ctrl-A Ctrl-X.\n"
                 "\n"
                 "Options:\n"
                 "  -b, --baud <rate>       Baud rate (default: 115200)\n"
                 "  -d, --databits <7|8>    Data bits (default: 8)\n"
                 "  -s, --stopbits <1|2>    Stop bits (default: 1)\n"
                 "  -p, --parity <n|e|o>    Parity none/even/odd (default: n)\n"
                 "  -e, --echo              Local echo (stdin -> stdout)\n"
                 "  -a, --escape <char>     Escape prefix char (default: Ctrl-A, use ^A)\n"
                 "      --send <file>       Send file to port and exit (no TTY)\n"
                 "  -h, --help              Show this help\n"
                 "\n"
                 "Escape commands (after prefix key):\n"
                 "  Ctrl-X                  Quit\n"
                 "  Ctrl-H                  Help\n"
                 "  <prefix>                Send literal prefix byte\n"
                 "\n"
                 "Examples:\n"
                 "  %s /dev/ttyUSB0\n"
                 "  %s -b 1000000 /dev/ttyS0\n"
                 "  %s --send firmware.bin /dev/ttyUSB0\n",
                 prog,
                 prog,
                 prog,
                 prog);
}

bool parseCaretChar(const char* text, char* out)
{
    if (!text || !text[0]) {
        return false;
    }
    if (text[0] == '^' && text[1] != '\0' && text[2] == '\0') {
        const char c = text[1];
        if (c >= '?' && c <= '_') {
            *out = static_cast<char>(c - '@');
            return true;
        }
        if (c == '@') {
            *out = '\0';
            return true;
        }
    }
    if (text[1] == '\0') {
        *out = text[0];
        return true;
    }
    return false;
}

bool parseParity(const char* text, lpio::SerialParity* out)
{
    if (!text || !out) {
        return false;
    }
    if (std::strcmp(text, "n") == 0 || std::strcmp(text, "none") == 0) {
        *out = lpio::SerialParity::None;
        return true;
    }
    if (std::strcmp(text, "e") == 0 || std::strcmp(text, "even") == 0) {
        *out = lpio::SerialParity::Even;
        return true;
    }
    if (std::strcmp(text, "o") == 0 || std::strcmp(text, "odd") == 0) {
        *out = lpio::SerialParity::Odd;
        return true;
    }
    return false;
}

bool parseOptions(int argc, char* argv[], Options* opts)
{
    int positional = 0;

    for (int i = 1; i < argc; ++i) {
        const char* arg = argv[i];

        if (std::strcmp(arg, "-h") == 0 || std::strcmp(arg, "--help") == 0) {
            opts->showHelp = true;
            return true;
        }

        if (std::strcmp(arg, "-b") == 0 || std::strcmp(arg, "--baud") == 0) {
            if (++i >= argc) {
                return false;
            }
            opts->baudRate = static_cast<uint32_t>(std::strtoul(argv[i], nullptr, 10));
            continue;
        }

        if (std::strcmp(arg, "-d") == 0 || std::strcmp(arg, "--databits") == 0) {
            if (++i >= argc) {
                return false;
            }
            opts->dataBits = static_cast<uint8_t>(std::strtoul(argv[i], nullptr, 10));
            continue;
        }

        if (std::strcmp(arg, "-s") == 0 || std::strcmp(arg, "--stopbits") == 0) {
            if (++i >= argc) {
                return false;
            }
            opts->stopBits = static_cast<uint8_t>(std::strtoul(argv[i], nullptr, 10));
            continue;
        }

        if (std::strcmp(arg, "-p") == 0 || std::strcmp(arg, "--parity") == 0) {
            if (++i >= argc) {
                return false;
            }
            if (!parseParity(argv[i], &opts->parity)) {
                return false;
            }
            continue;
        }

        if (std::strcmp(arg, "-e") == 0 || std::strcmp(arg, "--echo") == 0) {
            opts->localEcho = true;
            continue;
        }

        if (std::strcmp(arg, "-a") == 0 || std::strcmp(arg, "--escape") == 0) {
            if (++i >= argc) {
                return false;
            }
            if (!parseCaretChar(argv[i], &opts->escapeChar)) {
                return false;
            }
            continue;
        }

        if (std::strcmp(arg, "--send") == 0) {
            if (++i >= argc) {
                return false;
            }
            opts->sendFile = argv[i];
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

lpio::SerialPort openPort(const Options& opts)
{
    auto builder = lpio::SerialPort::on(opts.device).baudRate(opts.baudRate);

    if (opts.dataBits == 7) {
        builder.dataBits(7);
    } else if (opts.dataBits == 8) {
        builder.dataBits(8);
    } else {
        throw std::runtime_error("data bits must be 7 or 8");
    }

    if (opts.stopBits == 1) {
        builder.stopBits(1);
    } else if (opts.stopBits == 2) {
        builder.stopBits(2);
    } else {
        throw std::runtime_error("stop bits must be 1 or 2");
    }

    builder.parity(opts.parity);
    return builder.open();
}

int sendFile(lpio::SerialPort& port, const std::string& path)
{
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        throw std::runtime_error("cannot open file: " + path);
    }

    std::vector<std::byte> buf(4096);
    while (in) {
        in.read(reinterpret_cast<char*>(buf.data()),
                static_cast<std::streamsize>(buf.size()));
        const auto n = in.gcount();
        if (n <= 0) {
            break;
        }
        port.write(std::span<const std::byte>(buf.data(), static_cast<std::size_t>(n)));
    }

    port.flush();
    return 0;
}

char escapeDisplayChar(char c)
{
    if (c >= 1 && c <= 26) {
        return static_cast<char>(c + '@');
    }
    return c ? c : '?';
}

void printBanner(const Options& opts)
{
    char parityCh = 'N';
    switch (opts.parity) {
    case lpio::SerialParity::Even:
        parityCh = 'E';
        break;
    case lpio::SerialParity::Odd:
        parityCh = 'O';
        break;
    default:
        break;
    }

    const char esc = escapeDisplayChar(opts.escapeChar);

    std::fprintf(stderr,
                 "emcom: %s @ %u %u%c%u\n"
                 "escape: ^%c  quit: ^%c Ctrl-X  help: ^%c Ctrl-H\n",
                 opts.device.c_str(),
                 opts.baudRate,
                 opts.dataBits,
                 parityCh,
                 opts.stopBits,
                 esc,
                 esc,
                 esc);
}

void printEscapeHelp(char escape)
{
    const char esc = escapeDisplayChar(escape);
    std::fprintf(stderr,
                 "\n--- escape commands (^%c prefix) ---\n"
                 "  ^%c Ctrl-X   exit\n"
                 "  ^%c Ctrl-H   this help\n"
                 "  ^%c^%c       send literal prefix\n\n",
                 esc,
                 esc,
                 esc,
                 esc,
                 esc);
}

int runInteractive(lpio::SerialPort& port, const Options& opts)
{
    StdinRawMode tty;
    const bool     stdinIsTTY = tty.enter();

    std::atomic<bool> running {true};
    std::mutex        portMu;

    printBanner(opts);

    std::thread rxThread([&]() {
        std::vector<std::byte> buf(512);
        while (running) {
            std::size_t n = 0;
            try {
                std::lock_guard lock(portMu);
                n = port.read(buf);
            } catch (const std::exception& ex) {
                if (running) {
                    std::fprintf(stderr, "\nserial read error: %s\n", ex.what());
                }
                running = false;
                break;
            }

            if (n == 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                continue;
            }

            if (::write(STDOUT_FILENO, buf.data(), n) < 0) {
                running = false;
                break;
            }
        }
    });

    enum class EscState { Normal, GotEscape };

    EscState state = EscState::Normal;

    while (running) {
        unsigned char ch = 0;
        ssize_t       n  = 0;

        n = ::read(STDIN_FILENO, &ch, 1);

        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            break;
        }
        if (n == 0) {
            break;
        }

        if (state == EscState::Normal) {
            if (ch == static_cast<unsigned char>(opts.escapeChar)) {
                state = EscState::GotEscape;
                continue;
            }

            if (opts.localEcho) {
                (void)::write(STDOUT_FILENO, &ch, 1);
            }

            try {
                std::lock_guard lock(portMu);
                port.write(std::span<const std::byte>(reinterpret_cast<const std::byte*>(&ch), 1));
            } catch (const std::exception& ex) {
                std::fprintf(stderr, "\nserial write error: %s\n", ex.what());
                running = false;
            }
            continue;
        }

        // GotEscape
        state = EscState::Normal;

        if (ch == '\x18' || ch == '\x04' || ch == 'x' || ch == 'X') {
            running = false;
            break;
        }

        if (ch == '\x08' || ch == 'h' || ch == 'H') {
            printEscapeHelp(opts.escapeChar);
            continue;
        }

        if (ch == static_cast<unsigned char>(opts.escapeChar)) {
            try {
                std::lock_guard lock(portMu);
                const std::byte b = static_cast<std::byte>(opts.escapeChar);
                port.write(std::span<const std::byte>(&b, 1));
            } catch (const std::exception& ex) {
                std::fprintf(stderr, "\nserial write error: %s\n", ex.what());
                running = false;
            }
            continue;
        }

        // Unknown escape: send prefix + byte
        try {
            std::lock_guard lock(portMu);
            const std::byte prefix = static_cast<std::byte>(opts.escapeChar);
            port.write(std::span<const std::byte>(&prefix, 1));
            port.write(std::span<const std::byte>(reinterpret_cast<const std::byte*>(&ch), 1));
        } catch (const std::exception& ex) {
            std::fprintf(stderr, "\nserial write error: %s\n", ex.what());
            running = false;
        }
    }

    running = false;
    if (rxThread.joinable()) {
        rxThread.join();
    }

    std::fprintf(stderr, "\n");
    return 0;
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

    try {
        auto port = openPort(opts);

        if (opts.sendFile) {
            return sendFile(port, *opts.sendFile);
        }

        return runInteractive(port, opts);
    } catch (const std::exception& ex) {
        std::fprintf(stderr, "emcom: %s\n", ex.what());
        return 1;
    }
}
