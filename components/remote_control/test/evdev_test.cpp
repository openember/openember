// Exercise the real backend against a deterministic evdev syscall boundary.
#include "openember/remote_control/gamepad.hpp"
#include <cerrno>
#include <cstdarg>
#include <cstring>
#include <deque>
#include <fcntl.h>
#include <iostream>
#include <linux/input.h>
#include <poll.h>
#include <stdexcept>
#include <sys/ioctl.h>
#include <unistd.h>

namespace {
constexpr int kFd = 12345;
int initial_button = -1;
std::deque<input_event> events;
bool wrong_identity = false, broken_read = false, hangup = false, closed = false;
void Require(bool condition) { if (!condition) throw std::runtime_error("evdev expectation failed"); }
void Event(unsigned short type, unsigned short code, int value = 0) {
    input_event event{}; event.type = type; event.code = code; event.value = value; events.push_back(event);
}
}
extern "C" {
int __real_open(const char*, int, ...);
int __real_close(int);
ssize_t __real_read(int, void*, size_t);
int __real_poll(pollfd*, nfds_t, int);
int __real_ioctl(int, unsigned long, ...);
int __wrap_open(const char* path, int flags, ...) {
    if (std::strcmp(path, "/test/f710") == 0) return kFd;
    mode_t mode = 0;
    if (flags & O_CREAT) { va_list args; va_start(args, flags); mode = va_arg(args, int); va_end(args); }
    return __real_open(path, flags, mode);
}
int __wrap_close(int fd) { if (fd == kFd) { closed = true; return 0; } return __real_close(fd); }
ssize_t __wrap_read(int fd, void* buffer, size_t size) {
    if (fd != kFd) return __real_read(fd, buffer, size);
    if (broken_read) { errno = ENODEV; return -1; }
    if (events.empty()) { errno = EAGAIN; return -1; }
    std::memcpy(buffer, &events.front(), sizeof(input_event)); events.pop_front(); return sizeof(input_event);
}
int __wrap_poll(pollfd* fds, nfds_t count, int timeout) {
    if (count != 1 || fds[0].fd != kFd) return __real_poll(fds, count, timeout);
    fds[0].revents = hangup ? POLLHUP : 0; return hangup ? 1 : 0;
}
int __wrap_ioctl(int fd, unsigned long request, ...) {
    va_list args; va_start(args, request); void* out = va_arg(args, void*); va_end(args);
    if (fd != kFd) return __real_ioctl(fd, request, out);
    if (request == EVIOCGID) {
        auto& id = *static_cast<input_id*>(out); id = {};
        id.vendor = 0x046d; id.product = wrong_identity ? 0xc219 : 0xc21f; return 0;
    }
    if (_IOC_NR(request) == _IOC_NR(EVIOCGBIT(EV_KEY, 1))) {
        std::memset(out, 0xff, _IOC_SIZE(request)); return 0;
    }
    if (_IOC_NR(request) == _IOC_NR(EVIOCGKEY(1))) {
        std::memset(out, 0, _IOC_SIZE(request));
        if (initial_button >= 0) static_cast<unsigned char*>(out)[initial_button / 8] |= 1 << (initial_button % 8);
        return 0;
    }
    const auto code = _IOC_NR(request) - 0x40;
    if (code <= ABS_MAX) {
        auto& info = *static_cast<input_absinfo*>(out); info = {};
        if (code == ABS_Z || code == ABS_RZ) { info.minimum = 0; info.maximum = 255; }
        else if (code == ABS_HAT0X || code == ABS_HAT0Y) { info.minimum = -1; info.maximum = 1; }
        else { info.minimum = -32768; info.maximum = 32767; }
        return 0;
    }
    errno = EINVAL; return -1;
}
}
int main(int argc, char** argv) {
    try {
        if (argc != 2) throw std::runtime_error("case required");
        const std::string name = argv[1];
        namespace rc = openember::remote_control;
        if (name == "initial_x") initial_button = 0x133;
        if (name == "initial_y") initial_button = 0x134;
        openember::remote_control::F710Evdev device("/test/f710");
        if (name == "identity") {
            wrong_identity = true; Require(!device.Poll().connected && closed && !device.Error().empty());
        } else {
            auto state = device.Poll(); Require(state.connected && state.generation == 1);
            if (name == "buttons" || name == "initial_x" || name == "initial_y") {
                // Fixed Linux XInput wire codes, deliberately independent of backend constants.
                const std::array<unsigned short, rc::Index(rc::Button::Count)> codes{
                    0x130, 0x131, 0x133, 0x134, 0x136, 0x137, 0x13a, 0x13b, 0x13c, 0x13d, 0x13e};
                if (name != "buttons") {
                    const auto expected = rc::Index(name == "initial_x" ? rc::Button::X : rc::Button::Y);
                    for (std::size_t j = 0; j < state.buttons.size(); ++j) Require(state.buttons[j] == (j == expected));
                } else {
                    for (std::size_t i = 0; i < codes.size(); ++i) {
                        Event(EV_KEY, codes[i], 1); Event(EV_SYN, SYN_REPORT);
                        state = device.Poll();
                        for (std::size_t j = 0; j < state.buttons.size(); ++j) Require(state.buttons[j] == (j == i));
                        Event(EV_KEY, codes[i], 0); Event(EV_SYN, SYN_REPORT);
                        state = device.Poll(); for (bool button : state.buttons) Require(!button);
                    }
                }
            } else if (name == "packets") {
                Event(EV_ABS, ABS_Y, -32768); Event(EV_KEY, BTN_SOUTH, 1);
                // Partial packets must not become a published snapshot.
                Require(device.Poll().axes[1] == 0);
                Event(EV_ABS, ABS_Z, 255); Event(EV_SYN, SYN_REPORT);
                state = device.Poll(); Require(state.axes[1] == -1 && state.axes[4] == 1 && state.buttons[0]);
                Event(EV_ABS, ABS_Y, 32767); Event(EV_KEY, BTN_SOUTH, 0); Event(EV_SYN, SYN_REPORT);
                state = device.Poll(); Require(state.axes[1] == 1 && !state.buttons[0]);
            } else {
                if (name == "sync") Event(EV_SYN, SYN_DROPPED);
                else if (name == "overflow") for (int i = 0; i < 300; ++i) Event(EV_ABS, ABS_X, i);
                else if (name == "read") broken_read = true;
                else if (name == "hangup") hangup = true;
                else throw std::runtime_error("unknown case");
                state = device.Poll(); Require(!state.connected && closed && !device.Error().empty());
                for (double axis : state.axes) Require(axis == 0);
                for (bool button : state.buttons) Require(!button);
            }
        }
        return 0;
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
