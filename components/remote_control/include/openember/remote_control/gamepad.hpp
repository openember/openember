#pragma once

#include <array>
#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace openember::remote_control {
using Time = std::chrono::steady_clock::time_point;
enum class Axis { LeftX, LeftY, RightX, RightY, LeftTrigger, RightTrigger, HatX, HatY, Count };
enum class Button { A, B, X, Y, LB, RB, Back, Start, Guide, LeftStick, RightStick, Count };
constexpr std::size_t Index(Axis value) { return static_cast<std::size_t>(value); }
constexpr std::size_t Index(Button value) { return static_cast<std::size_t>(value); }
struct GamepadState {
    bool connected = false;
    std::uint64_t generation = 0;  // Changes on every successful device open.
    Time sampled_at{};             // Host observation time, NOT a wireless heartbeat.
    std::array<double, Index(Axis::Count)> axes{};  // Sticks/hats [-1,1], triggers [0,1].
    std::array<bool, Index(Button::Count)> buttons{};
};
class GamepadDevice {
 public:
    virtual ~GamepadDevice() = default;
    virtual GamepadState Poll() = 0;  // Single owner; bounded, nonblocking device read.
};

// Linux only. Empty path discovers exactly one F710 XInput device (046d:c21f).
// Rejects ambiguity; an explicit /dev/input/by-id/*-event-joystick is preferred.
class F710Evdev final : public GamepadDevice {
 public:
    explicit F710Evdev(std::string path = {});
    ~F710Evdev() override;
    F710Evdev(const F710Evdev&) = delete;
    F710Evdev& operator=(const F710Evdev&) = delete;
    GamepadState Poll() override;
    std::string Path() const;
    std::string Error() const;
 private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

struct ActionBinding {
    std::string action;
    std::vector<Button> buttons;  // All must be held; fires once on rising edge.
    bool require_deadman = true;
};
struct AxisBinding {
    std::string action;
    Axis axis;
    double scale = 1;
    bool require_deadman = true;  // False permits ready axes without holding the modifier; product FSM gates execution.
};
struct Mapping {
    Button deadman = Button::LB;
    double deadzone = 0.12;
    std::chrono::milliseconds max_sample_age{200};
    std::vector<ActionBinding> buttons;
    std::vector<AxisBinding> axes;
};
struct AxisAction { std::string action; double value; };
struct MappedInput {
    bool available = false;
    bool ready = false;  // Neutral sticks, released triggers and buttons seen after connection.
    bool deadman = false;
    bool reset = false;  // Product must discard pending intent on input/session loss.
    std::vector<std::string> actions;
    std::vector<AxisAction> axes;
};
class InputMapper {
 public:
    explicit InputMapper(Mapping mapping);
    MappedInput Update(const GamepadState& state, Time now);
 private:
    Mapping mapping_;
    std::uint64_t generation_ = 0;
    bool ready_ = false;
    bool available_ = false;
    std::vector<bool> previous_;
};
}  // namespace openember::remote_control
