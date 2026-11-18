#include "openember/remote_control/gamepad.hpp"

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <filesystem>
#include <fcntl.h>
#include <linux/input.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <unistd.h>

namespace openember::remote_control {
namespace {
constexpr std::array<unsigned, Index(Axis::Count)> kAxes{
    ABS_X, ABS_Y, ABS_RX, ABS_RY, ABS_Z, ABS_RZ, ABS_HAT0X, ABS_HAT0Y};
constexpr std::array<unsigned, Index(Button::Count)> kButtons{
    BTN_SOUTH, BTN_EAST, BTN_WEST, BTN_NORTH, BTN_TL, BTN_TR, BTN_SELECT,
    BTN_START, BTN_MODE, BTN_THUMBL, BTN_THUMBR};
using KeyBits = std::array<unsigned char, (KEY_MAX + 8) / 8>;
bool Bit(const KeyBits& bits, unsigned code) { return (bits[code / 8] & (1 << (code % 8))) != 0; }
double Normalize(int value, const input_absinfo& info, bool trigger) {
    const double bounded = std::clamp(static_cast<double>(value), static_cast<double>(info.minimum),
                                    static_cast<double>(info.maximum));
    if (trigger) return (bounded - info.minimum) / (static_cast<double>(info.maximum) - info.minimum);
    // Preserve zero despite the asymmetric signed range of XInput sticks.
    return bounded >= 0 ? bounded / info.maximum : bounded / -static_cast<double>(info.minimum);
}
}  // namespace

struct F710Evdev::Impl {
    std::string requested, path, error;
    int fd = -1;
    Time next_open{};
    GamepadState state, pending;
    std::array<input_absinfo, Index(Axis::Count)> ranges{};
    ~Impl() { if (fd >= 0) close(fd); }
    void Drop(std::string reason) {
        if (fd >= 0) close(fd);
        fd = -1; path.clear(); error = std::move(reason);
        const auto generation = state.generation;
        state = {}; state.generation = generation; pending = state;
        next_open = std::chrono::steady_clock::now() + std::chrono::seconds(1);
    }
    bool Open(const std::string& candidate) {
        const int device = open(candidate.c_str(), O_RDONLY | O_NONBLOCK | O_CLOEXEC);
        if (device < 0) { error = candidate + ": " + std::strerror(errno); return false; }
        input_id identity{};
        KeyBits supported{}, pressed{};
        auto reject = [&] { close(device); error = candidate + ": expected F710 XInput layout (046d:c21f)"; return false; };
        if (ioctl(device, EVIOCGID, &identity) < 0 || identity.vendor != 0x046d || identity.product != 0xc21f ||
            ioctl(device, EVIOCGBIT(EV_KEY, sizeof(supported)), supported.data()) < 0 ||
            ioctl(device, EVIOCGKEY(sizeof(pressed)), pressed.data()) < 0) return reject();
        GamepadState initial;
        for (std::size_t i = 0; i < kButtons.size(); ++i) {
            if (!Bit(supported, kButtons[i])) return reject();
            initial.buttons[i] = Bit(pressed, kButtons[i]);
        }
        for (std::size_t i = 0; i < kAxes.size(); ++i) {
            auto& info = ranges[i];
            const bool trigger = i == Index(Axis::LeftTrigger) || i == Index(Axis::RightTrigger);
            if (ioctl(device, EVIOCGABS(kAxes[i]), &info) < 0 || info.maximum <= info.minimum ||
                (!trigger && (info.minimum >= 0 || info.maximum <= 0))) return reject();
            initial.axes[i] = Normalize(info.value, info, trigger);
        }
        fd = device; path = candidate; error.clear();
        initial.generation = state.generation + 1;
        initial.connected = true;
        state = pending = initial;
        return true;
    }
    void Discover() {
        if (!requested.empty()) { Open(requested); return; }
        std::vector<std::string> candidates;
        std::error_code ec;
        std::filesystem::directory_iterator iterator("/dev/input", ec);
        if (ec) { error = "cannot enumerate /dev/input: " + ec.message(); return; }
        for (const auto& entry : iterator) {
            if (entry.path().filename().string().rfind("event", 0) != 0) continue;
            // Metadata probe does not consume events or grab the device.
            const int probe = open(entry.path().c_str(), O_RDONLY | O_NONBLOCK | O_CLOEXEC);
            if (probe < 0) continue;
            input_id id{};
            if (ioctl(probe, EVIOCGID, &id) >= 0 && id.vendor == 0x046d && id.product == 0xc21f)
                candidates.push_back(entry.path().string());
            close(probe);
        }
        if (candidates.size() != 1) {
            error = candidates.empty() ? "no readable F710 XInput device; check receiver, X mode and permissions" :
                                        "multiple F710 devices; specify an explicit event device";
            return;
        }
        Open(candidates.front());
    }
};
F710Evdev::F710Evdev(std::string path) : impl_(std::make_unique<Impl>()) { impl_->requested = std::move(path); }
F710Evdev::~F710Evdev() = default;
std::string F710Evdev::Path() const { return impl_->path; }
std::string F710Evdev::Error() const { return impl_->error; }
GamepadState F710Evdev::Poll() {
    auto& p = *impl_;
    const auto now = std::chrono::steady_clock::now();
    if (p.fd < 0 && now >= p.next_open) {
        p.next_open = now + std::chrono::seconds(1);
        p.Discover();
    }
    if (p.fd >= 0) {
        pollfd descriptor{p.fd, POLLIN, 0};
        const auto result = poll(&descriptor, 1, 0);
        if ((result < 0 && errno != EINTR) || (descriptor.revents & (POLLHUP | POLLERR | POLLNVAL)))
            p.Drop("F710 device disconnected or poll failed");
    }
    // Bound drain time: an overfull input stream invalidates this session.
    for (unsigned count = 0; p.fd >= 0; ++count) {
        input_event event{};
        const auto size = read(p.fd, &event, sizeof(event));
        if (size < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) break;
        if (size < 0 && errno == EINTR) break;
        if (size != sizeof(event)) { p.Drop("F710 event read failed"); break; }
        if (count >= 256 || (event.type == EV_SYN && event.code == SYN_DROPPED)) {
            p.Drop("F710 input synchronization lost; reconnect and neutral required"); break;
        }
        if (event.type == EV_ABS) {
            for (std::size_t i = 0; i < kAxes.size(); ++i)
                if (event.code == kAxes[i]) p.pending.axes[i] = Normalize(event.value, p.ranges[i],
                    i == Index(Axis::LeftTrigger) || i == Index(Axis::RightTrigger));
        } else if (event.type == EV_KEY) {
            for (std::size_t i = 0; i < kButtons.size(); ++i)
                if (event.code == kButtons[i]) p.pending.buttons[i] = event.value != 0;
        } else if (event.type == EV_SYN && event.code == SYN_REPORT) p.state = p.pending;
    }
    p.state.sampled_at = std::chrono::steady_clock::now();
    return p.state;
}
}  // namespace openember::remote_control
