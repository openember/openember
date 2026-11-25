#pragma once
#include "openember/context.hpp"
#include "openember/sensor/imu/driver_registry.hpp"
#include "openember/msgs/sensor/v1/imu.pb.h"
#include <functional>

namespace openember::sensor::link {
namespace wire = openember::msgs::sensor::v1;
struct Credential {
    std::string robot_id, source_id, secret;
    // observe, use, or maintenance. Keys must be distinct per principal.
    std::string role = "observe";
};
std::string Token();
Credential LoadCredential(const std::string&);
void CreateCredential(const std::string& path, const std::string& robot,
                      const std::string& source, const std::string& role);
void RequireLoopback(const std::string&);

struct ServiceOptions {
    std::shared_ptr<openember::Context> context;
    std::string robot_id, source_node, source_instance, host_clock_domain;
    ImuDriverConfig device;
    std::vector<Credential> access;
    // Empty string grants maintenance; a missing/stale/unsafe product state
    // returns its reason. Must be bounded and safe on the acquisition thread.
    std::function<std::string()> maintenance_check;
    std::function<std::unique_ptr<IImu>(std::uint32_t)> factory;
};

// Owns the sole driver. HI calls ReadSample; queued device commands execute on
// that same thread. Configuration and protected use leases exclude one another.
class ManagedImu final : public IImu {
public:
    ManagedImu(std::unique_ptr<IImu>, ServiceOptions);
    ~ManagedImu() override;
    const SensorInfo& Info() const override;
    hardware::DeviceStatus Status() const override;
    hardware::Result<void> Start() override;
    void Stop() noexcept override;
    hardware::Result<ImuSample> ReadSample(std::chrono::milliseconds) override;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

// Own from one worker thread. It pins each request to an authenticated service
// session and never automatically retries a device command.
class Client {
public:
    Client(const std::string& locator, Credential, std::string sensor_id);
    ~Client();
    wire::ImuReply State();
    wire::ImuRequest Request(wire::ImuOperation, const std::string& lease = {});
    wire::ImuReply Call(const wire::ImuRequest&);
    wire::ImuReply Result(const std::string& id);
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace openember::sensor::link
