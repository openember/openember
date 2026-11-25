#include "openember/confirmed_change.hpp"
#include <stdexcept>
using namespace std::chrono_literals;
void Check(bool v) {
  if (!v)
    throw std::runtime_error("confirmation timer contract");
}
int main() {
  openember::ConfirmedChange c;
  auto now = openember::ConfirmedChange::Clock::now();
  int committed = 0, recovered = 0;
  auto arm = [&] {
    c.Arm(now + 10s, [&] { ++committed; }, [&] { ++recovered; });
  };
  arm();
  Check(!c.Poll(now + 9s));
  Check(c.Confirm(now + 9s));
  Check(committed == 1 && recovered == 0);
  Check(!c.Confirm(now + 9s));
  arm();
  Check(!c.Confirm(now + 10s));
  Check(committed == 1 && recovered == 1);
  Check(!c.Poll(now + 11s));
  arm();
  Check(c.Poll(now + 11s));
  Check(recovered == 2);
  c.Rollback();
  Check(recovered == 2);
  c.Arm(
      now + 10s, [] { throw std::runtime_error("commit"); },
      [&] { ++recovered; });
  try {
    c.Confirm(now);
  } catch (...) {
  }
  Check(c.Status() == openember::ConfirmedChange::State::Pending);
  c.Rollback();
  Check(recovered == 3);
  c.Arm(now, [] {}, [] { throw std::runtime_error("recovery"); });
  try {
    c.Poll(now);
  } catch (...) {
  }
  Check(c.Status() == openember::ConfirmedChange::State::RecoveryFailed);
}
