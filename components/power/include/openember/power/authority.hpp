#pragma once
#include "openember/power/controller.hpp"
#include <memory>
namespace openember::power {
enum class Operation { kSetLevel, kEnable, kDisable };
struct Permission {
    std::string resource_id;
    Operation operation = Operation::kSetLevel;
};
struct ReadinessPermit {
    std::string robot_id, power_id, power_session, clock_domain, issuer_session;
    std::uint64_t sequence = 0, issued_monotonic_ns = 0, expires_monotonic_ns = 0;
    bool simulation = false;
    std::vector<Permission> operations;
};
// Local authorization authority, NOT an authentication protocol. These mutators
// are for trusted process code only, never directly exposed as Link services.
// Remote readiness must be authenticated before ApplyTrustedPermit is called.
class Authority {
public:
    Authority(std::string robot, std::string power_id, std::string power_session,
              std::string clock_domain, bool simulation = false);
    ~Authority();
    Authority(const Authority&) = delete;
    Authority& operator=(const Authority&) = delete;
    void SetPermissions(const std::string& principal, std::vector<Permission>);
    // Session transitions come from the authenticated manager adapter. Same
    // session does not reset the replay fence. A new session invalidates permits.
    void BindTrustedIssuer(const std::string& session);
    bool ApplyTrustedPermit(ReadinessPermit, std::uint64_t now);
    void RevokeReadiness();
    // Irreversible for this service session, including captured contexts.
    void Close();
    // Caller identity must already be authenticated by the service adapter.
    Context ForAuthenticated(const std::string& principal) const;
private:
    struct State;
    std::shared_ptr<State> state_;
};
}
