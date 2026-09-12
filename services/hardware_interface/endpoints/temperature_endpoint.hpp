#pragma once

#include <atomic>
#include <mutex>
#include <string>
#include <thread>

#include "openember/msgs/sensor/v1/sensor.pb.h"
#include "openember/publisher.hpp"
#include "openember/sensor/mock_temperature.hpp"
#include "openember/services/hardware_interface/endpoint.hpp"

namespace openember::services::hardware_interface {

class TemperatureEndpoint final : public HardwareEndpoint {
public:
    TemperatureEndpoint(EndpointConfig config, EndpointContext context);
    ~TemperatureEndpoint() override;

    const std::string& Id() const override;
    bool Configure() override;
    bool Start() override;
    void Stop() noexcept override;
    EndpointStatus Status() const override;

private:
    void Run();
    void RecordError(const std::string& message);
    void SetError(const std::string& message);

    EndpointConfig config_;
    EndpointContext context_;
    openember::sensor::MockTemperature temperature_;
    openember::Publisher<openember::msgs::sensor::v1::TemperatureSample> publisher_;
    std::atomic_bool running_{false};
    std::thread thread_;
    mutable std::mutex status_mutex_;
    EndpointStatus status_;
};

}  // namespace openember::services::hardware_interface
