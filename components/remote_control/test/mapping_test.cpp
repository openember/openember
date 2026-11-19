#include "openember/remote_control/gamepad.hpp"
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace rc = openember::remote_control;
using namespace std::chrono_literals;
void Require(bool condition) { if (!condition) throw std::runtime_error("expectation failed"); }
rc::Mapping Mapping() {
    rc::Mapping map;
    map.buttons = {{"arm", {rc::Button::LB, rc::Button::Start}, true},
                   {"stop", {rc::Button::Back}, false}, {"pose", {rc::Button::A}, true}};
    map.axes = {{"forward", rc::Axis::LeftY, -0.3}};
    return map;
}
int main(int argc, char** argv) {
    try {
        if (argc != 2) throw std::runtime_error("case required");
        const std::string name = argv[1];
        auto time = rc::Time{} + 10s;
        rc::GamepadState state;
        state.connected = true; state.generation = 1; state.sampled_at = time;
        auto mapping = Mapping();
        if (name == "independent_axes") mapping.axes[0].require_deadman = false;
        rc::InputMapper mapper(mapping);
        auto update = [&] { return mapper.Update(state, time); };
        auto button = [&](rc::Button b, bool held) { state.buttons[rc::Index(b)] = held; };
        if (name == "neutral") {
            button(rc::Button::LB, true); button(rc::Button::Start, true);
            Require(!update().ready); Require(update().actions.empty());
            state.buttons.fill(false); state.axes[rc::Index(rc::Axis::LeftTrigger)] = 0.8;
            Require(!update().ready);
            state.axes.fill(0); Require(update().ready);
            button(rc::Button::LB, true); button(rc::Button::Start, true);
            Require(update().actions == std::vector<std::string>{"arm"});
        } else if (name == "edge") {
            Require(update().ready); button(rc::Button::LB, true); button(rc::Button::Start, true);
            Require(update().actions.size() == 1); Require(update().actions.empty());
            button(rc::Button::Start, false); update(); button(rc::Button::Start, true);
            Require(update().actions.size() == 1);
        } else if (name == "deadman") {
            update(); button(rc::Button::A, true); Require(update().actions.empty());
            button(rc::Button::LB, true); Require(update().actions.empty());
            button(rc::Button::A, false); update(); button(rc::Button::A, true);
            Require(update().actions == std::vector<std::string>{"pose"});
            state.axes[rc::Index(rc::Axis::LeftY)] = -1; Require(update().axes[0].value == 0.3);
            button(rc::Button::LB, false); Require(update().axes[0].value == 0);
            button(rc::Button::Back, true); Require(update().actions == std::vector<std::string>{"stop"});
        } else if (name == "axes") {
            update(); button(rc::Button::LB, true);
            state.axes[rc::Index(rc::Axis::LeftY)] = 0.1; Require(update().axes[0].value == 0);
            state.axes[rc::Index(rc::Axis::LeftY)] = -0.56;
            Require(std::abs(update().axes[0].value - 0.15) < 1e-12);
            state.axes[rc::Index(rc::Axis::LeftY)] = 1; Require(update().axes[0].value == -0.3);
        } else if (name == "independent_axes") {
            state.axes[rc::Index(rc::Axis::LeftY)] = -1;
            auto out = update(); Require(!out.ready && out.axes[0].value == 0);
            state.axes.fill(0); Require(update().ready);
            state.axes[rc::Index(rc::Axis::LeftY)] = -.56;
            Require(std::abs(update().axes[0].value - .15) < 1e-12);
            button(rc::Button::LB, true); Require(std::abs(update().axes[0].value - .15) < 1e-12);
            button(rc::Button::LB, false); Require(std::abs(update().axes[0].value - .15) < 1e-12);
            state.connected = false; out = update(); Require(out.reset && out.axes[0].value == 0);
            state.connected = true; out = update(); Require(!out.ready && out.axes[0].value == 0);
            state.axes.fill(0); Require(update().ready);
            state.axes[rc::Index(rc::Axis::LeftY)] = -1; Require(update().axes[0].value == .3);
            time += 201ms; out = update(); Require(!out.available && out.axes[0].value == 0);
        } else if (name == "stale") {
            update(); button(rc::Button::LB, true); update(); time += 201ms;
            auto out = update(); Require(out.reset && !out.available && !out.ready && out.axes[0].value == 0);
            Require(!update().reset); state.sampled_at = time; Require(!update().ready);
            state.buttons.fill(false); Require(update().ready);
        } else if (name == "reconnect") {
            update(); button(rc::Button::LB, true); button(rc::Button::Start, true); update();
            state.generation++; auto out = update(); Require(out.reset && !out.ready && out.actions.empty());
            state.buttons.fill(false); Require(update().ready);
            state.connected = false; Require(update().reset);
            state.connected = true; button(rc::Button::A, true); Require(!update().ready);
        } else if (name == "invalid") {
            update(); state.axes[0] = std::numeric_limits<double>::quiet_NaN(); Require(!update().available);
            state.axes[0] = 2; Require(!update().available);
            state.axes[0] = 0; state.axes[4] = -1; Require(!update().available);
            state.axes[4] = 0; state.sampled_at = time + 1ms; Require(!update().available);
            state.sampled_at = time; state.generation = 0; Require(!update().available);
        } else if (name == "config") {
            auto rejected = [](rc::Mapping map) {
                try { rc::InputMapper invalid(std::move(map)); } catch (const std::invalid_argument&) { return true; }
                return false;
            };
            auto map = Mapping(); map.deadzone = 1; Require(rejected(map));
            map = Mapping(); map.axes[0].scale = std::numeric_limits<double>::infinity(); Require(rejected(map));
            map = Mapping(); map.buttons.push_back(map.buttons[0]); Require(rejected(map));
            map = Mapping(); map.buttons[0].buttons.clear(); Require(rejected(map));
            map = Mapping(); map.max_sample_age = 0ms; Require(rejected(map));
        } else throw std::runtime_error("unknown case");
        return 0;
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
