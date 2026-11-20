#pragma once
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace openember::system_state {
enum class Phase {
  Initializing = 1,
  Standby,
  Running,
  Suspended,
  Recovering,
  ShuttingDown
};
enum class Health { Unknown = 1, Normal, Warning, Degraded, Fault };
enum class Actuation { Unknown = 1, Disarmed, Armed, EmergencyStop };
enum class Response { Observe, Degrade, Fault };
enum class Condition { Clear, Active, Unknown };
struct Rule {
  std::string source, code;
  Response response = Response::Observe;
};
struct Alert {
  std::string source, device, session, code;
  Condition condition = Condition::Unknown;
  bool error = false;
  std::string Key() const;
};
struct Observation {
  std::string publisher, instance;
  std::uint64_t revision = 0, heartbeat = 0, stale_after_ns = 0;
  bool online = true;
  std::vector<Alert> alerts;
};
struct Snapshot {
  Phase phase = Phase::Initializing;
  Health health = Health::Unknown;
  Actuation actuation = Actuation::Unknown;
  std::uint32_t active = 0, unknown = 0;
  std::vector<std::string> unavailable;
};
// Single-owner, bounded reducer. Time is the receiver's monotonic clock.
// It returns decisions/state only; it never commands hardware or restarts
// nodes.
class Manager {
public:
  Manager(std::vector<std::string> publishers, std::vector<Rule> rules = {});
  bool State(const Observation &observation, std::uint64_t received_ns);
  bool Event(const Observation &observation, std::uint64_t received_ns);
  void Operation(Phase phase, Actuation actuation, bool fault,
                 std::uint64_t received_ns, std::uint64_t stale_after_ns);
  Snapshot Read(std::uint64_t now) const;

private:
  struct Publisher {
    std::string instance;
    std::vector<std::string> retired;
    std::uint64_t revision = 0, heartbeat = 0, received = 0, stale = 0;
    bool online = false, complete = false;
    std::map<std::string, Alert> alerts;
  };
  std::map<std::string, Publisher> publishers_;
  std::vector<Rule> rules_;
  Phase phase_ = Phase::Initializing;
  Actuation actuation_ = Actuation::Unknown;
  bool fault_ = false;
  std::uint64_t operation_received_ = 0, operation_stale_ = 0;
};
} // namespace openember::system_state
