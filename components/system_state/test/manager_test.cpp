#include "openember/system_state/manager.hpp"
#include <iostream>
#include <stdexcept>
namespace s = openember::system_state;
void Check(bool b) {
  if (!b)
    throw std::runtime_error("system state contract failed");
}
int main() {
  try {
    constexpr std::uint64_t tick = 100000000;
    s::Manager m({"motion"}, {{"tilt", "angle", s::Response::Observe},
                              {"motor", "heat", s::Response::Fault}});
    Check(m.Read(tick).health == s::Health::Unknown);
    m.Operation(s::Phase::Running, s::Actuation::Armed, false, tick, 10 * tick);
    Check(m.Read(tick).health == s::Health::Unknown);
    s::Observation o{"motion", "instance-1", 0, 1, 5 * tick, true, {}};
    Check(m.State(o, tick));
    Check(m.Read(tick).health == s::Health::Normal);
    s::Alert a{"tilt", "robot", "session", "angle", s::Condition::Active, true};
    o.revision = 1;
    o.alerts = {a};
    Check(m.Event(o, tick));
    auto state = m.Read(tick);
    Check(state.health == s::Health::Warning &&
          state.phase == s::Phase::Running && state.active == 1);
    Check(!m.Event(o, tick));
    o.revision = 3;
    Check(m.Event(o, tick));
    Check(m.Read(tick).health == s::Health::Unknown);
    o.heartbeat = 2;
    Check(m.State(o, 2 * tick));
    Check(m.Read(2 * tick).health == s::Health::Warning);
    o.alerts = {};
    o.heartbeat = 3;
    Check(m.State(o, 3 * tick));
    Check(m.Read(3 * tick).active == 0);
    o.revision = 4;
    o.alerts = {
        {"motor", "robot", "session", "heat", s::Condition::Active, false}};
    Check(m.Event(o, 3 * tick));
    Check(m.Read(3 * tick).health == s::Health::Fault);
    o.heartbeat = 4;
    o.alerts.front().condition = s::Condition::Unknown;
    Check(m.State(o, 4 * tick));
    Check(m.Read(4 * tick).health == s::Health::Unknown &&
          m.Read(4 * tick).unknown == 1);
    o.instance = "instance-2";
    o.revision = 0;
    o.heartbeat = 1;
    o.alerts = {};
    Check(m.State(o, 5 * tick));
    o.instance = "instance-1";
    o.heartbeat = 5;
    Check(!m.State(o, 5 * tick));
    o.instance = "instance-2";
    Check(m.Read(11 * tick).health == s::Health::Unknown);
    o.heartbeat = 2;
    o.online = false;
    Check(m.State(o, 6 * tick));
    Check(m.Read(6 * tick).health == s::Health::Unknown);
    o.online = true;
    o.heartbeat = 3;
    Check(m.State(o, 7 * tick));
    Check(!m.State(o, 8 * tick));
    Check(m.Read(13 * tick).health == s::Health::Unknown);
    Check(m.Read(13 * tick).actuation == s::Actuation::Unknown);
    bool rejected = false;
    try {
      s::Manager bad({"duplicate", "duplicate"});
    } catch (...) {
      rejected = true;
    }
    Check(rejected);
    std::cout << "SYSTEM_STATE_TEST passed snapshots=1 events=1 gap=1 stale=1 "
                 "restart=1 dimensions=1\n";
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
