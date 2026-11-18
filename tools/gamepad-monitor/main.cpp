#include "openember/remote_control/gamepad.hpp"
#include <csignal>
#include <iostream>
#include <stdexcept>
#include <thread>

namespace rc = openember::remote_control;
namespace { volatile std::sig_atomic_t stopped = 0; void Stop(int) { stopped = 1; } }
int main(int argc, char** argv) {
    try {
        std::string path;
        int seconds = 0;
        for (int i = 1; i < argc; ++i) {
            const std::string argument = argv[i];
            if (argument == "--help") {
                std::cout << "openember_gamepad_monitor [--device /dev/input/...event...] [--seconds N]\n"
                             "F710 XInput only; axes: LX LY RX RY LT RT hatX hatY; buttons: A B X Y LB RB Back Start Guide LS RS\n";
                return 0;
            }
            if (i + 1 >= argc) throw std::invalid_argument("option needs a value");
            const std::string value = argv[++i];
            if (argument == "--device") path = value;
            else if (argument == "--seconds") {
                std::size_t used = 0; seconds = std::stoi(value, &used);
                if (used != value.size() || seconds <= 0 || seconds > 86400) throw std::invalid_argument("seconds must be 1..86400");
            } else throw std::invalid_argument("unknown option: " + argument);
        }
        std::signal(SIGINT, Stop); std::signal(SIGTERM, Stop);
        rc::F710Evdev device(path);
        rc::InputMapper mapper(rc::Mapping{});
        const auto start = std::chrono::steady_clock::now();
        auto print_at = start;
        std::string previous_error;
        bool found = false;
        while (!stopped && (!seconds || std::chrono::steady_clock::now() - start < std::chrono::seconds(seconds))) {
            const auto state = device.Poll();
            const auto now = std::chrono::steady_clock::now();
            const auto input = mapper.Update(state, now);
            found = found || state.connected;
            if (device.Error() != previous_error) {
                previous_error = device.Error();
                if (!previous_error.empty()) std::cerr << previous_error << '\n';
            }
            if (now >= print_at) {
                std::cout << "GAMEPAD connected=" << input.available << " ready=" << input.ready
                          << " generation=" << state.generation << " path=" << device.Path() << " axes=";
                for (double axis : state.axes) std::cout << axis << ',';
                std::cout << " buttons=";
                for (bool button : state.buttons) std::cout << button;
                std::cout << std::endl;
                print_at = now + std::chrono::milliseconds(100);
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        return found ? 0 : 2;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
