#include "openember/power/authority.hpp"
#include "openember/power/mock.hpp"
#include "openember/hardware/timestamp.hpp"
#include <iostream>
#include <stdexcept>
namespace p = openember::power;
void Check(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
class DispatchDevice final : public p::IPowerDevice {
public:
    p::MockPowerDevice device{{"pmu",{{"motor",p::ResourceKind::kChannel,true,true,false,true}}, {}}};
    std::function<void()> before_dispatch;
    openember::hardware::Result<void> Start() override { return device.Start(); }
    void Stop() noexcept override { device.Stop(); }
    p::Capabilities GetCapabilities() const override { return device.GetCapabilities(); }
    openember::hardware::Result<p::Snapshot> Read() override { return device.Read(); }
    p::ExecutionResult Execute(const p::Target& t, std::uint64_t deadline) override { return device.Execute(t,deadline); }
    p::ExecutionResult ExecuteChecked(const p::Target& t, std::uint64_t deadline, p::ExecutionCheck check) override {
        if (before_dispatch) before_dispatch();
        return IPowerDevice::ExecuteChecked(t,deadline,std::move(check));
    }
};
int main() {
    try {
        const auto now = openember::hardware::SteadyTimeNs();
        p::Authority authority("robot", "pmu", "session", "host");
        authority.SetPermissions("operator", {{"motor", p::Operation::kEnable}, {"fan", p::Operation::kSetLevel}});
        const auto context = authority.ForAuthenticated("operator");
        const p::Target on{"motor", p::ResourceKind::kChannel, 1}, off{"motor", p::ResourceKind::kChannel, 0};
        Check(context.authorize(off, false, now).reason == p::Reason::kUnauthorized, "enable permission granted disable");
        Check(context.authorize(on, true, now).reason == p::Reason::kNotReady, "missing permit granted power");
        authority.BindTrustedIssuer("manager");
        p::ReadinessPermit permit{"robot", "pmu", "session", "host", "manager", 1, now, now+100000000, false,
            {{"motor", p::Operation::kEnable}}};
        const auto invalid = [&](auto edit) {
            auto bad=permit; edit(bad); Check(!authority.ApplyTrustedPermit(bad,now), "invalid permit accepted");
        };
        invalid([](auto& q){q.robot_id="other";});
        invalid([](auto& q){q.power_id="other";});
        invalid([](auto& q){q.power_session="old";});
        invalid([](auto& q){q.clock_domain="remote";});
        invalid([](auto& q){q.issuer_session="spoof";});
        invalid([](auto& q){q.simulation=true;});
        invalid([](auto& q){q.issued_monotonic_ns+=1;});
        invalid([](auto& q){q.expires_monotonic_ns=q.issued_monotonic_ns+1000000001;});
        invalid([](auto& q){q.expires_monotonic_ns=q.issued_monotonic_ns;});
        Check(authority.ApplyTrustedPermit(permit,now), "valid permit rejected");
        Check(context.authorize(on,true,now).Allowed(), "valid permit ignored");
        Check(!authority.ApplyTrustedPermit(permit,now), "permit sequence replay accepted");
        authority.BindTrustedIssuer("manager");
        Check(!authority.ApplyTrustedPermit(permit,now), "same issuer reset replay fence");
        Check(context.authorize(on,true,permit.expires_monotonic_ns).reason == p::Reason::kNotReady, "expired permit granted power");
        Check(context.authorize(on,false,now).Allowed(), "permit expiry revoked operation permission");
        authority.SetPermissions("operator", {});
        Check(context.authorize(on,true,now).reason == p::Reason::kUnauthorized, "captured context ignored policy revocation");
        authority.SetPermissions("operator", {{"motor",p::Operation::kEnable}});
        authority.RevokeReadiness();
        Check(!context.authorize(on,true,now).Allowed(), "revocation ignored");
        Check(!authority.ApplyTrustedPermit(permit,now), "revocation reset replay fence");
        permit.sequence=2;
        Check(authority.ApplyTrustedPermit(permit,now), "new permit rejected");
        authority.BindTrustedIssuer("new-manager");
        Check(!context.authorize(on,true,now).Allowed(), "manager restart retained permit");
        Check(!authority.ApplyTrustedPermit(permit,now), "old manager granted power");
        p::MockPowerDevice device({"pmu",{{"motor",p::ResourceKind::kChannel,true,true,false,true}}, {}});
        device.Start(); p::Controller c(device,"session","host");
        p::Request q{"1","pmu","session","host",now+10000000000ULL,on};
        permit.issuer_session="new-manager"; permit.sequence=1;
        Check(authority.ApplyTrustedPermit(permit,now), "new manager permit rejected");
        Check(c.Handle(q,context,now).outcome==p::Outcome::kConfirmed, "authorized power-on failed");
        authority.RevokeReadiness();
        Check(c.Handle(q,context,now).outcome==p::Outcome::kConfirmed && device.ExecutionCount()==1,"cached operation needed readiness or replayed");
        q.request_id="2";
        Check(c.Handle(q,context,now).reason==p::Reason::kNotReady,"new operation used expired intent");
        Check(device.Read().Value().resources[0].observed_level==1,"permit loss automatically cut power");
        authority.SetPermissions("operator",{{"motor",p::Operation::kDisable}});
        q.target=off;
        Check(c.Handle(q,context,now).reason==p::Reason::kNotReady,"normal disable skipped coordination permit");
        DispatchDevice queued; queued.Start(); p::Controller guarded(queued,"session","host");
        authority.SetPermissions("operator",{{"motor",p::Operation::kEnable}});
        permit.sequence=2; permit.issued_monotonic_ns=openember::hardware::SteadyTimeNs();
        permit.expires_monotonic_ns=permit.issued_monotonic_ns+100000000;
        Check(authority.ApplyTrustedPermit(permit,permit.issued_monotonic_ns),"dispatch setup permit rejected");
        q.target=on; q.request_id="dispatch-revoke";
        queued.before_dispatch=[&]{authority.SetPermissions("operator",{});};
        Check(guarded.Handle(q,context,openember::hardware::SteadyTimeNs()).reason==p::Reason::kUnauthorized &&
            queued.device.ExecutionCount()==0,"dispatch skipped live permission check");
        authority.SetPermissions("operator",{{"motor",p::Operation::kEnable}});
        queued.before_dispatch=[&]{authority.RevokeReadiness();}; q.request_id="dispatch-readiness";
        Check(guarded.Handle(q,context,openember::hardware::SteadyTimeNs()).reason==p::Reason::kNotReady &&
            queued.device.ExecutionCount()==0,"dispatch skipped live readiness check");
        // A context retained past its authority's lifetime cannot authorize.
        p::Context orphan;
        { p::Authority temp("r","p","s","d"); temp.SetPermissions("op",{{"fan",p::Operation::kSetLevel}}); orphan=temp.ForAuthenticated("op"); }
        Check(!orphan.authorize({"fan",p::ResourceKind::kFan,0},false,now).Allowed(),"orphan context granted access");
        authority.Close();
        Check(!context.authorize(on,false,now).Allowed(),"closed authority accepted new work");
        const auto closed_now=openember::hardware::SteadyTimeNs();
        permit.sequence=3; permit.issued_monotonic_ns=closed_now; permit.expires_monotonic_ns=closed_now+100000000;
        Check(!authority.ApplyTrustedPermit(permit,closed_now),"manager update revived closed authority");
        std::cout << "POWER_AUTHORITY_OK\n";
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
