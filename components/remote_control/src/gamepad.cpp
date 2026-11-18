#include "openember/remote_control/gamepad.hpp"

#include <algorithm>
#include <cmath>
#include <set>
#include <stdexcept>

namespace openember::remote_control {
InputMapper::InputMapper(Mapping mapping) : mapping_(std::move(mapping)) {
    if (Index(mapping_.deadman) >= Index(Button::Count) || !std::isfinite(mapping_.deadzone) ||
        mapping_.deadzone < 0 || mapping_.deadzone >= 1 || mapping_.max_sample_age.count() <= 0)
        throw std::invalid_argument("invalid input mapping limits");
    std::set<std::string> names;
    for (const auto& binding : mapping_.buttons) {
        if (binding.action.empty() || !names.insert(binding.action).second || binding.buttons.empty())
            throw std::invalid_argument("empty or duplicate action binding");
        std::set<Button> buttons;
        for (auto button : binding.buttons)
            if (Index(button) >= Index(Button::Count) || !buttons.insert(button).second)
                throw std::invalid_argument("invalid or duplicate button");
    }
    for (const auto& binding : mapping_.axes)
        if (binding.action.empty() || !names.insert(binding.action).second ||
            Index(binding.axis) >= Index(Axis::Count) || !std::isfinite(binding.scale))
            throw std::invalid_argument("invalid axis binding");
    previous_.resize(mapping_.buttons.size(), false);
}

MappedInput InputMapper::Update(const GamepadState& state, Time now) {
    MappedInput out;
    bool valid = state.connected && state.generation != 0 && state.sampled_at <= now &&
                 now - state.sampled_at <= mapping_.max_sample_age;
    for (std::size_t i = 0; i < state.axes.size(); ++i)
        valid = valid && std::isfinite(state.axes[i]) && state.axes[i] <= 1 &&
                state.axes[i] >= ((i == Index(Axis::LeftTrigger) || i == Index(Axis::RightTrigger)) ? 0 : -1);
    out.reset = available_ && (!valid || generation_ != state.generation);
    if (!valid || !available_ || generation_ != state.generation) {
        ready_ = false;
        std::fill(previous_.begin(), previous_.end(), false);
    }
    available_ = valid;
    generation_ = state.generation;
    out.available = valid;
    if (valid && !ready_) {
        const bool neutral = std::all_of(state.axes.begin(), state.axes.end(), [&](double v) {
            return std::abs(v) <= mapping_.deadzone;
        }) && std::none_of(state.buttons.begin(), state.buttons.end(), [](bool b) { return b; });
        ready_ = neutral;
    }
    out.ready = valid && ready_;
    out.deadman = out.ready && state.buttons[Index(mapping_.deadman)];
    for (std::size_t i = 0; i < mapping_.buttons.size(); ++i) {
        const auto& binding = mapping_.buttons[i];
        const bool held = valid && std::all_of(binding.buttons.begin(), binding.buttons.end(), [&](Button b) {
            return state.buttons[Index(b)];
        });
        if (out.ready && held && !previous_[i] && (!binding.require_deadman || out.deadman))
            out.actions.push_back(binding.action);
        previous_[i] = held;
    }
    for (const auto& binding : mapping_.axes) {
        double value = out.deadman ? state.axes[Index(binding.axis)] : 0;
        value = std::abs(value) <= mapping_.deadzone ? 0 :
            std::copysign((std::abs(value) - mapping_.deadzone) / (1 - mapping_.deadzone), value);
        out.axes.push_back({binding.action, value == 0 ? 0 : value * binding.scale});
    }
    return out;
}
}  // namespace openember::remote_control
