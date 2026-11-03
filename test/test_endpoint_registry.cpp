#include "openember/services/hardware_interface/hardware_interface_app.hpp"
#include "openember/init.hpp"
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <algorithm>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
namespace hi = openember::services::hardware_interface;
void Check(bool ok, const char* reason) { if (!ok) throw std::runtime_error(reason); }
template<class F> void Reject(F action) {
    bool rejected = false;
    try { action(); } catch (const std::exception&) { rejected = true; }
    Check(rejected, "expected rejection");
}
struct Trace { std::vector<std::string> starts, stops; bool runtime_error = false; };
class Endpoint final : public hi::HardwareEndpoint {
public:
    Endpoint(hi::EndpointConfig config, std::shared_ptr<Trace> trace)
        : config_(std::move(config)), trace_(std::move(trace)) { status_.endpoint_id = config_.endpoint_id; }
    const std::string& Id() const override { return config_.endpoint_id; }
    std::vector<hi::EndpointSampleTopic> AdditionalSampleTopics() const override {
        if (Id() == "bad_topic") return {{config_.topic, "test.Extra"}};
        if (Id() == "empty_type") return {{config_.topic+"/extra", ""}};
        return {{config_.topic+"/extra", "test.Extra"}};
    }
    bool Configure() override { return config_.endpoint_id != "bad_config"; }
    bool Start() override {
        trace_->starts.push_back(Id());
        if (Id() == "throw") throw std::runtime_error("injected start exception");
        status_.state = hi::EndpointState::kRunning;
        return Id() != "bad_start";
    }
    void Stop() noexcept override { trace_->stops.push_back(Id()); status_.state = hi::EndpointState::kStopped; }
    hi::EndpointStatus Status() const override {
        auto status = status_;
        if (trace_->runtime_error) status.state = hi::EndpointState::kError;
        return status;
    }
private:
    hi::EndpointConfig config_;
    std::shared_ptr<Trace> trace_;
    hi::EndpointStatus status_;
};
hi::EndpointConfig Config(const std::string& id = "ok") {
    hi::EndpointConfig config;
    config.endpoint_id = id; config.type = "custom"; config.driver = "product";
    config.topic = "/test/" + id; config.critical = true;
    return config;
}
hi::EndpointRegistry Registry(const std::shared_ptr<Trace>& trace) {
    hi::EndpointRegistry registry;
    registry.Register("custom", "product", {"test.Sample", ""},
        [trace](const hi::EndpointConfig& config, const hi::EndpointContext&) {
            return std::make_unique<Endpoint>(config, trace);
        });
    return registry;
}
std::string Listen() {
    const int fd = socket(AF_INET, SOCK_STREAM, 0);
    Check(fd >= 0, "socket failed");
    sockaddr_in address{};
    address.sin_family = AF_INET; address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    socklen_t length = sizeof(address);
    const bool ok = bind(fd, reinterpret_cast<sockaddr*>(&address), length) == 0 &&
        getsockname(fd, reinterpret_cast<sockaddr*>(&address), &length) == 0;
    close(fd);
    Check(ok, "port allocation failed");
    return "tcp/127.0.0.1:" + std::to_string(ntohs(address.sin_port));
}
}
int main(int argc, char** argv) {
    try {
        Check(argc == 2, "case required");
        const std::string which = argv[1];
        auto trace = std::make_shared<Trace>();
        auto registry = Registry(trace);
        if (which == "registry") {
            auto config = Config();
            Check(registry.Create(config, {})->Id() == "ok", "product factory not used");
            Check(registry.Describe(config)->sample_type == "test.Sample", "metadata mismatch");
            Check(registry.Create(config, {})->AdditionalSampleTopics().size() == 1, "extra topic metadata missing");
            Reject([&] { registry.Create(Config("bad_topic"), {}); });
            Reject([&] { registry.Create(Config("empty_type"), {}); });
            Reject([&] { registry.Register("custom", "product", {"test.Other", ""},
                [](const hi::EndpointConfig&, const hi::EndpointContext&) { return nullptr; }); });
            Check(registry.Describe(config)->sample_type == "test.Sample", "duplicate replaced registration");
            config.command_topic = "/unsafe";
            Reject([&] { registry.Create(config, {}); });
            config = Config(); config.driver = "typo";
            Reject([&] { registry.Create(config, {}); });
            registry.Register("custom", "null", {"test.Sample", ""},
                [](const hi::EndpointConfig&, const hi::EndpointContext&) { return nullptr; });
            config.driver = "null"; Reject([&] { registry.Create(config, {}); });
            registry.Register("custom", "wrong_id", {"test.Sample", ""},
                [trace](const hi::EndpointConfig&, const hi::EndpointContext&) {
                    return std::make_unique<Endpoint>(Config("different"), trace);
                });
            config.driver = "wrong_id"; Reject([&] { registry.Create(config, {}); });
            Check(trace->stops.back() == "different", "bad factory result not stopped");
            return 0;
        }
        openember::RuntimeOptions runtime;
        runtime.link.profile = openember::link::Profile::kRouter; runtime.link.listen = Listen();
        openember::Init(runtime);
        {
            auto node = openember::CreateNode("registry_test");
            hi::HardwareInterfaceConfig config; config.endpoints.push_back(Config());
            if (which == "duplicate") config.endpoints.push_back(Config());
            else if (which == "topic_collision") { config.endpoints.push_back(Config("other")); config.endpoints.back().topic="/test/ok/extra"; }
            else if (which == "noncritical_failure") { config.endpoints[0] = Config("bad_start"); config.endpoints[0].critical = false; }
            else if (which == "nan") config.endpoints[0].publish_rate_hz = std::numeric_limits<double>::quiet_NaN();
            else if (which == "bad_config" || which == "bad_start" || which == "throw")
                config.endpoints.push_back(Config(which));
            else if (which == "missing") { config.endpoints.push_back(Config("missing")); config.endpoints.back().driver = "missing"; }
            else Check(which == "lifecycle" || which == "runtime_error", "unknown case");
            hi::HardwareInterfaceApp app(node, config, {}, registry);
            const bool ok = app.Start();
            if (which == "lifecycle" || which == "runtime_error") {
                Check(ok, "custom endpoint failed");
                Check(!app.Start() && trace->starts.size() == 1, "repeated Start accepted");
                if (which == "runtime_error") { trace->runtime_error = true; Reject([&] { app.Spin(); }); }
                app.Stop(); app.Stop();
                Check(!app.Start(), "restart accepted with stale callback ownership");
            } else {
                Check(!ok, "invalid lifecycle succeeded");
                if (which == "duplicate" || which == "nan" || which == "missing" || which == "topic_collision")
                    Check(trace->starts.empty(), "invalid topology partially started");
                else if (which == "noncritical_failure") Check(trace->starts.size() == 1, "noncritical start not attempted");
                else Check(std::find(trace->stops.begin(), trace->stops.end(), "ok") != trace->stops.end(),
                           "earlier endpoint not rolled back");
            }
        }
        openember::Shutdown();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n'; openember::Shutdown(); return 1;
    }
}
