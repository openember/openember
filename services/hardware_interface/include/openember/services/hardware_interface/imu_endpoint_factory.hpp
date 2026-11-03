#pragma once

#include <memory>
#include "openember/sensor/imu.hpp"
#include "openember/services/hardware_interface/endpoint.hpp"

namespace openember::services::hardware_interface {

// Product-owned drivers can reuse the public IMU publisher without adding SDKs
// or driver names to OpenEmber. Ownership transfers to the endpoint. The driver
// must support bounded ReadSample and concurrent Stop cancellation. Publication
// follows successful reads; this is not a rate limiter, estimator, or safety gate.
std::unique_ptr<HardwareEndpoint> MakeImuEndpoint(
    EndpointConfig config, EndpointContext context,
    std::unique_ptr<sensor::IImu> driver);

} // namespace openember::services::hardware_interface
