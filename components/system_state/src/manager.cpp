#include "openember/system_state/manager.hpp"
#include <algorithm>
#include <set>
#include <stdexcept>
#include <tuple>
namespace openember::system_state {
namespace {
bool Id(const std::string &s) {
  if (s.empty() || s.size() > 256)
    return false;
  return std::none_of(s.begin(), s.end(),
                      [](unsigned char c) { return c < 33 || c == 127; });
}
bool Valid(const Observation &o) {
  if (!Id(o.publisher) || !Id(o.instance) || o.alerts.size() > 256)
    return false;
  std::set<std::string> keys;
  for (const auto &a : o.alerts)
    if (!Id(a.source) || !Id(a.device) || !Id(a.session) || !Id(a.code) ||
        (a.condition != Condition::Clear && a.condition != Condition::Active &&
         a.condition != Condition::Unknown) ||
        !keys.insert(a.Key()).second)
      return false;
  return true;
}
} // namespace
std::string Alert::Key() const {
  return source + '\0' + device + '\0' + session + '\0' + code;
}
Manager::Manager(std::vector<std::string> publishers, std::vector<Rule> rules)
    : rules_(std::move(rules)) {
  if (publishers.empty() || publishers.size() > 32 || rules_.size() > 256)
    throw std::invalid_argument("invalid system state bounds");
  for (const auto &name : publishers)
    if (!Id(name) || !publishers_.emplace(name, Publisher{}).second)
      throw std::invalid_argument("invalid/duplicate system publisher");
  std::set<std::tuple<std::string, std::string, std::string>> unique;
  for (const auto &rule : rules_)
    if (!Id(rule.source) || !Id(rule.code) ||
        (!rule.device.empty() && !Id(rule.device)) ||
        (rule.response != Response::Observe &&
         rule.response != Response::Degrade &&
         rule.response != Response::Fault) ||
        !unique.emplace(rule.source, rule.code, rule.device).second)
      throw std::invalid_argument("invalid/duplicate alert response rule");
}
bool Manager::State(const Observation &o, std::uint64_t now) {
  if (!Valid(o) || !now || !o.heartbeat || o.stale_after_ns < 50000000 ||
      o.stale_after_ns > 60000000000ULL)
    return false;
  auto found = publishers_.find(o.publisher);
  if (found == publishers_.end())
    return false;
  auto &p = found->second;
  if (p.instance != o.instance) {
    if (std::find(p.retired.begin(), p.retired.end(), o.instance) !=
            p.retired.end() ||
        p.retired.size() >= 16)
      return false;
    if (!p.instance.empty())
      p.retired.push_back(p.instance);
    p.instance = o.instance;
    p.revision = p.heartbeat = 0;
    p.complete = false;
  }
  if (o.revision < p.revision || o.heartbeat <= p.heartbeat || now < p.received)
    return false;
  p.alerts.clear();
  for (const auto &a : o.alerts)
    p.alerts.emplace(a.Key(), a);
  p.revision = o.revision;
  p.heartbeat = o.heartbeat;
  p.received = now;
  p.stale = o.stale_after_ns;
  p.online = o.online;
  p.complete = true;
  return true;
}
bool Manager::Event(const Observation &o, std::uint64_t now) {
  if (!Valid(o) || o.alerts.size() != 1 || !now)
    return false;
  auto found = publishers_.find(o.publisher);
  if (found == publishers_.end())
    return false;
  auto &p = found->second;
  // A full snapshot establishes each incarnation. Events do not renew liveness.
  if (p.instance != o.instance || !p.complete || o.revision <= p.revision ||
      now < p.received)
    return false;
  const auto key = o.alerts.front().Key();
  if (!p.alerts.count(key) && p.alerts.size() >= 256) {
    p.complete = false;
    return false;
  }
  if (o.revision != p.revision + 1)
    p.complete = false;
  p.alerts[key] = o.alerts.front();
  p.revision = o.revision;
  return true;
}
void Manager::Operation(Phase phase, Actuation actuation, bool fault,
                        std::uint64_t now, std::uint64_t stale) {
  if (!now || !stale || stale > 60000000000ULL || now < operation_received_ ||
      phase < Phase::Initializing || phase > Phase::ShuttingDown ||
      actuation < Actuation::Unknown || actuation > Actuation::EmergencyStop)
    return;
  phase_ = phase;
  actuation_ = actuation;
  fault_ = fault;
  operation_received_ = now;
  operation_stale_ = stale;
}
Snapshot Manager::Read(std::uint64_t now) const {
  Snapshot out;
  out.phase = phase_;
  out.actuation = actuation_;
  out.health = Health::Normal;
  if (!operation_received_ || now < operation_received_ ||
      now - operation_received_ > operation_stale_) {
    out.phase = Phase::Initializing;
    out.actuation = Actuation::Unknown;
    out.health = Health::Unknown;
    out.unavailable.push_back("operation");
  } else if (fault_)
    out.health = Health::Fault;
  for (const auto &item : publishers_) {
    const auto &p = item.second;
    if (!p.complete || !p.online || !p.received || now < p.received ||
        now - p.received > p.stale) {
      out.unavailable.push_back(item.first);
      if (out.health != Health::Fault)
        out.health = Health::Unknown;
      continue;
    }
    for (const auto &entry : p.alerts) {
      const auto &alert = entry.second;
      const Rule *selected = nullptr;
      for (const auto &rule : rules_)
        if (rule.source == alert.source && rule.code == alert.code &&
            (rule.device.empty() || rule.device == alert.device) &&
            (!selected || !rule.device.empty()))
          selected = &rule;
      if (alert.condition == Condition::Unknown) {
        ++out.unknown;
        if ((!selected || selected->unknown_blocks) &&
            out.health != Health::Fault)
          out.health = Health::Unknown;
      } else if (alert.condition == Condition::Active) {
        ++out.active;
        Response response = alert.error ? Response::Degrade : Response::Observe;
        if (selected)
          response = selected->response;
        const auto health = response == Response::Fault     ? Health::Fault
                            : response == Response::Degrade ? Health::Degraded
                                                            : Health::Warning;
        if (health == Health::Fault ||
            (out.health != Health::Unknown && health > out.health))
          out.health = health;
      }
    }
  }
  return out;
}
} // namespace openember::system_state
