#include "adapters/sensor_message_adapter.hpp"

namespace openember::services::hardware_interface {

namespace {

void FillHeader(openember::msgs::common::v1::Header* header,
                const MessageHeaderContext& context,
                std::uint64_t sequence) {
    header->set_source_node(context.source_node);
    header->set_source_instance(context.source_instance);
    header->set_robot_id(context.robot_id);
    header->set_sequence(sequence);
    header->set_timestamp_unix_ns(context.timestamp_unix_ns);
}

void FillVector(openember::msgs::sensor::v1::Vector3* output,
                const openember::sensor::Vector3& input) {
    output->set_x(input.x);
    output->set_y(input.y);
    output->set_z(input.z);
}

openember::msgs::sensor::v1::GnssFixType ToProtoFixType(
    openember::sensor::GnssFixType type) {
    switch (type) {
    case openember::sensor::GnssFixType::kNoFix:
        return openember::msgs::sensor::v1::GNSS_FIX_TYPE_NO_FIX;
    case openember::sensor::GnssFixType::kFix2D:
        return openember::msgs::sensor::v1::GNSS_FIX_TYPE_FIX_2D;
    case openember::sensor::GnssFixType::kFix3D:
        return openember::msgs::sensor::v1::GNSS_FIX_TYPE_FIX_3D;
    case openember::sensor::GnssFixType::kRtkFloat:
        return openember::msgs::sensor::v1::GNSS_FIX_TYPE_RTK_FLOAT;
    case openember::sensor::GnssFixType::kRtkFixed:
        return openember::msgs::sensor::v1::GNSS_FIX_TYPE_RTK_FIXED;
    }
    return openember::msgs::sensor::v1::GNSS_FIX_TYPE_UNSPECIFIED;
}

}  // namespace

openember::msgs::sensor::v1::ImuSample ToProto(
    const openember::sensor::ImuSample& sample,
    const MessageHeaderContext& context) {
    openember::msgs::sensor::v1::ImuSample msg;
    FillHeader(msg.mutable_header(), context, sample.sequence);
    msg.set_sensor_id(sample.sensor_id);
    msg.set_frame_id(sample.frame_id);
    msg.set_sample_time_monotonic_ns(sample.sample_time_monotonic_ns);
    msg.set_sequence(sample.sequence);
    FillVector(msg.mutable_acceleration_mps2(), sample.acceleration_mps2);
    FillVector(msg.mutable_angular_velocity_radps(), sample.angular_velocity_radps);
    FillVector(msg.mutable_magnetic_field_tesla(), sample.magnetic_field_tesla);
    msg.set_temperature_celsius(sample.temperature_celsius);
    return msg;
}

openember::msgs::sensor::v1::TemperatureSample ToProto(
    const openember::sensor::TemperatureSample& sample,
    const MessageHeaderContext& context) {
    openember::msgs::sensor::v1::TemperatureSample msg;
    FillHeader(msg.mutable_header(), context, sample.sequence);
    msg.set_sensor_id(sample.sensor_id);
    msg.set_frame_id(sample.frame_id);
    msg.set_sample_time_monotonic_ns(sample.sample_time_monotonic_ns);
    msg.set_sequence(sample.sequence);
    msg.set_temperature_celsius(sample.temperature_celsius);
    return msg;
}

openember::msgs::sensor::v1::GnssFix ToProto(
    const openember::sensor::GnssFix& fix,
    const MessageHeaderContext& context) {
    openember::msgs::sensor::v1::GnssFix msg;
    FillHeader(msg.mutable_header(), context, fix.sequence);
    msg.set_sensor_id(fix.sensor_id);
    msg.set_frame_id(fix.frame_id);
    msg.set_sample_time_monotonic_ns(fix.sample_time_monotonic_ns);
    msg.set_sequence(fix.sequence);
    msg.set_fix_type(ToProtoFixType(fix.fix_type));
    msg.set_latitude_deg(fix.latitude_deg);
    msg.set_longitude_deg(fix.longitude_deg);
    msg.set_altitude_m(fix.altitude_m);
    FillVector(msg.mutable_velocity_enu_mps(), fix.velocity_enu_mps);
    msg.set_satellites_used(fix.satellites_used);
    msg.set_horizontal_accuracy_m(fix.horizontal_accuracy_m);
    msg.set_vertical_accuracy_m(fix.vertical_accuracy_m);
    return msg;
}

}  // namespace openember::services::hardware_interface
