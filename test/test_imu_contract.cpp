#include "openember/sensor/imu_validation.hpp"
#include "openember/sensor/mock_imu.hpp"
#include "openember/services/hardware_interface/sensor_message_adapter.hpp"
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
namespace sensor = openember::sensor;
namespace hi = openember::services::hardware_interface;
void Check(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
sensor::ImuSample Good() {
    sensor::ImuSample sample;
    sample.sensor_id = "imu"; sample.frame_id = "sensor"; sample.reference_frame_id = "reference";
    sample.mounting_revision = "mount"; sample.clock_domain = "session";
    sample.sample_time_monotonic_ns = 42; // Intentionally distinct device sample clock.
    sample.receive_time_monotonic_ns = 1000; sample.sequence = 0;
    const auto norm = std::sqrt(30.0);
    sample.orientation = {1 / norm, 2 / norm, 3 / norm, 4 / norm}; sample.orientation_valid = true;
    sample.acceleration_mps2 = {1, 2, 3}; sample.acceleration_valid = true;
    sample.angular_velocity_radps = {4, 5, 6}; sample.angular_velocity_valid = true;
    sample.temperature_celsius = 25; sample.temperature_valid = true;
    return sample;
}
}
int main(int argc, char** argv) {
    try {
        Check(argc == 2, "case required"); const std::string which = argv[1];
        auto sample = Good();
        if (which == "adapter") {
            const auto message = hi::ToProto(sample, {"node", "instance", "robot", 1234});
            Check(message.header().source_instance() == "instance" && message.header().timestamp_unix_ns() == 1234,
                  "publication header lost");
            const auto decoded = hi::FromProto(message);
            Check(decoded.sample_time_monotonic_ns == 42 && decoded.receive_time_monotonic_ns == 1000 &&
                decoded.clock_domain == "session" && decoded.sequence == 0 &&
                decoded.orientation.w == sample.orientation.w && decoded.orientation.x == sample.orientation.x &&
                decoded.orientation.y == sample.orientation.y && decoded.orientation.z == sample.orientation.z &&
                decoded.acceleration_mps2.y == 2 && decoded.angular_velocity_radps.z == 6 &&
                decoded.mounting_revision == "mount" && decoded.reference_frame_id == "reference" &&
                decoded.temperature_valid && !decoded.magnetic_field_valid &&
                sensor::IsFreshImu(decoded, "session", 1000, 100), "IMU conversion lost fields");
            openember::msgs::sensor::v1::ImuSample legacy;
            legacy.set_sensor_id("old"); legacy.set_sequence(42);
            Check(!sensor::IsFreshImu(hi::FromProto(legacy), "session", 1000, 100), "legacy became valid");
            auto absent = message; absent.clear_acceleration_mps2();
            Check(!hi::FromProto(absent).acceleration_valid, "absent acceleration became valid zero");
            absent = message; absent.clear_angular_velocity_radps();
            Check(!hi::FromProto(absent).angular_velocity_valid, "absent gyro became valid zero");
            sample.orientation = {2, 0, 0, 0};
            const auto invalid = hi::FromProto(hi::ToProto(sample, {}));
            Check(invalid.orientation.w == 2 && !sensor::IsFreshImu(invalid, "session", 1000, 100),
                  "adapter normalized invalid quaternion");
        } else if (which == "validation") {
            Check(!sensor::IsFreshImu({}, "session", 1000, 100), "default valid");
            Check(sensor::IsFreshImu(sample, "session", 1100, 100), "boundary rejected");
            Check(!sensor::IsFreshImu(sample, "session", 1101, 100) &&
                !sensor::IsFreshImu(sample, "session", 999, 100) &&
                !sensor::IsFreshImu(sample, "other", 1000, 100) &&
                !sensor::IsFreshImu(sample, "session", 1000, 0), "invalid time/domain accepted");
            for (int i = 0; i < 13; ++i) {
                auto bad = Good();
                switch (i) {
                    case 0: bad.orientation_valid = false; break;
                    case 1: bad.orientation.w = std::numeric_limits<double>::quiet_NaN(); break;
                    case 2: bad.orientation.w = 2; break;
                    case 3: bad.acceleration_valid = false; break;
                    case 4: bad.angular_velocity_valid = false; break;
                    case 5: bad.acceleration_mps2.z = std::numeric_limits<double>::infinity(); break;
                    case 6: bad.angular_velocity_radps.x = std::numeric_limits<double>::quiet_NaN(); break;
                    case 7: bad.temperature_celsius = std::numeric_limits<double>::quiet_NaN(); break;
                    case 8: bad.magnetic_field_valid = true; bad.magnetic_field_tesla.x = std::numeric_limits<double>::infinity(); break;
                    case 9: bad.sensor_id.clear(); break;
                    case 10: bad.frame_id.clear(); break;
                    case 11: bad.reference_frame_id.clear(); break;
                    case 12: bad.mounting_revision.clear(); break;
                }
                Check(!sensor::IsFreshImu(bad, "session", 1000, 100), "invalid channel/metadata accepted");
            }
            sample.magnetic_field_tesla.x = std::numeric_limits<double>::quiet_NaN();
            Check(sensor::IsFreshImu(sample, "session", 1000, 100), "unavailable magnetic channel was required");
        } else if (which == "tracker") {
            sensor::ImuTracker tracker("session");
            Check(tracker.Accept(sample, 1000, 100), "sequence zero first sample rejected");
            Check(!tracker.Accept(sample, 1050, 100), "duplicate accepted");
            sample.receive_time_monotonic_ns = 1050;
            Check(!tracker.Accept(sample, 1050, 100), "repeated sequence refreshed");
            sample.sequence = 1; sample.receive_time_monotonic_ns = 999;
            Check(!tracker.Accept(sample, 1050, 100), "timestamp rollback accepted");
            Check(!tracker.Fresh(1101, 100), "duplicates kept old sample fresh");
            sample.receive_time_monotonic_ns = 1102;
            Check(tracker.Accept(sample, 1102, 100), "recovery rejected");
            for (int i = 0; i < 5; ++i) {
                auto changed = sample; changed.sequence = 2; changed.receive_time_monotonic_ns = 1103;
                switch (i) {
                    case 0: changed.clock_domain = "new-session"; break;
                    case 1: changed.sensor_id = "other"; break;
                    case 2: changed.frame_id = "other"; break;
                    case 3: changed.reference_frame_id = "other"; break;
                    case 4: changed.mounting_revision = "other"; break;
                }
                Check(!tracker.Accept(changed, 1103, 100), "identity change accepted");
            }
            Check(tracker.Sample().receive_time_monotonic_ns == 1102, "rejection updated timestamp");
            sample.clock_domain = "new-session"; sample.sequence = 0;
            sensor::ImuTracker fresh("new-session");
            Check(fresh.Accept(sample, 1102, 100), "explicit new session rejected");
        } else if (which == "mock") {
            sensor::MockImu imu;
            Check(imu.Start().Ok(), "mock start failed");
            auto first = imu.ReadSample(std::chrono::milliseconds(100));
            Check(first.Ok() && first.Value().acceleration_valid && first.Value().angular_velocity_valid &&
                first.Value().temperature_valid && !first.Value().orientation_valid && !first.Value().magnetic_field_valid &&
                first.Value().receive_time_monotonic_ns > 0 && !first.Value().clock_domain.empty(), "mock validity wrong");
            imu.Stop(); Check(imu.Start().Ok(), "mock restart failed");
            auto next = imu.ReadSample(std::chrono::milliseconds(100));
            Check(next.Ok() && next.Value().clock_domain != first.Value().clock_domain && next.Value().sequence == 0,
                  "mock restart reused domain");
            imu.Stop();
        } else throw std::invalid_argument("unknown case");
        return 0;
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
