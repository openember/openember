/*
 * Copyright (c) 2022-2026, OpenEmber Team
 * SPDX-License-Identifier: Apache-2.0
 *
 * OE-AFP (Agri Fieldbus Protocol) register map — compile-time contract.
 * Design owner: docs/design/agri-fieldbus-protocol.md
 * Instance topology (buses / units / poll): configs/agri_fieldbus.yaml
 */

#pragma once

#include <cstdint>
#include <string>

namespace agri_demo {
namespace afp {

// --- Protocol identity -------------------------------------------------------

constexpr std::uint16_t kProtoMagic = 0xAE01;
constexpr std::uint16_t kProtoVersion = 0x0100;  // major=1, minor=0
constexpr const char* kProtoId = "oe-afp/1.0";

constexpr std::uint16_t ProtoMajor(std::uint16_t version) {
    return static_cast<std::uint16_t>(version >> 8);
}
constexpr std::uint16_t ProtoMinor(std::uint16_t version) {
    return static_cast<std::uint16_t>(version & 0xFFu);
}

// --- Device class (reg 0x0002) -----------------------------------------------

enum class DeviceClass : std::uint16_t {
    kUnknown = 0,
    kIoCard = 1,
    kWirelessNode = 2,
    kGatewayVirtualSlave = 3,
};

// --- Capability bits (reg 0x0004 / uplink present_mask) -----------------------

constexpr std::uint16_t kCapAirTemp = 1u << 0;
constexpr std::uint16_t kCapAirHumidity = 1u << 1;
constexpr std::uint16_t kCapIlluminance = 1u << 2;
constexpr std::uint16_t kCapSoilMoisture = 1u << 3;
constexpr std::uint16_t kCapSoilTemp = 1u << 4;
constexpr std::uint16_t kCapCo2 = 1u << 5;

constexpr std::uint16_t kCapClimateBasic =
    kCapAirTemp | kCapAirHumidity | kCapIlluminance;
constexpr std::uint16_t kCapSoilBasic = kCapSoilMoisture | kCapSoilTemp;
constexpr std::uint16_t kCapDemoDefault =
    kCapClimateBasic | kCapSoilBasic;  // no CO2 by default

// --- Status bits (reg 0x0005) ------------------------------------------------

constexpr std::uint16_t kStatusFault = 1u << 0;
constexpr std::uint16_t kStatusDegraded = 1u << 1;
constexpr std::uint16_t kStatusBusy = 1u << 2;

// --- Quality bits (channel +6) -----------------------------------------------

constexpr std::uint16_t kQualityGood = 1u << 0;
constexpr std::uint16_t kQualityStale = 1u << 1;
constexpr std::uint16_t kQualityOutOfRange = 1u << 2;
constexpr std::uint16_t kQualitySensorFault = 1u << 3;
constexpr std::uint16_t kQualityNotInstalled = 1u << 4;
constexpr std::uint16_t kQualityCommLost = 1u << 5;

// --- last_error (reg 0x000A) -------------------------------------------------

enum class LastError : std::uint16_t {
    kOk = 0,
    kFrameError = 1,
    kSampleFail = 2,
    kBadConfig = 3,
    kCalibrationMissing = 4,
};

// --- Device info block (PDU addresses 0x0000–0x000F) -------------------------

constexpr std::uint16_t kRegProtoMagic = 0x0000;
constexpr std::uint16_t kRegProtoVersion = 0x0001;
constexpr std::uint16_t kRegDeviceClass = 0x0002;
constexpr std::uint16_t kRegChannelCount = 0x0003;
constexpr std::uint16_t kRegCapsBits = 0x0004;
constexpr std::uint16_t kRegStatusBits = 0x0005;
constexpr std::uint16_t kRegUptimeSLo = 0x0006;
constexpr std::uint16_t kRegUptimeSHi = 0x0007;
constexpr std::uint16_t kRegHeartbeat = 0x0008;
constexpr std::uint16_t kRegSamplePeriodMs = 0x0009;
constexpr std::uint16_t kRegLastError = 0x000A;
constexpr std::uint16_t kDeviceInfoBase = 0x0000;
constexpr std::uint16_t kDeviceInfoCount = 16;

// --- Channel blocks ----------------------------------------------------------

constexpr std::uint16_t kChannelBase = 0x0100;
constexpr std::uint16_t kChannelStride = 0x0010;
constexpr std::uint16_t kChannelMax = 32;
constexpr std::uint16_t kChannelDemoDefault = 10;

constexpr std::uint16_t kChOffAirTempC10 = 0x0;
constexpr std::uint16_t kChOffAirHumidityPct10 = 0x1;
constexpr std::uint16_t kChOffIlluminanceLux = 0x2;
constexpr std::uint16_t kChOffSoilMoisturePct10 = 0x3;
constexpr std::uint16_t kChOffSoilTempC10 = 0x4;
constexpr std::uint16_t kChOffCo2Ppm = 0x5;
constexpr std::uint16_t kChOffQuality = 0x6;
constexpr std::uint16_t kChOffSeqLo = 0x7;
constexpr std::uint16_t kChOffSeqHi = 0x8;
constexpr std::uint16_t kChOffAgeMs = 0x9;

constexpr std::uint16_t ChannelBase(std::uint16_t channel) {
    return static_cast<std::uint16_t>(kChannelBase + channel * kChannelStride);
}

constexpr std::uint16_t ChannelReg(std::uint16_t channel, std::uint16_t offset) {
    return static_cast<std::uint16_t>(ChannelBase(channel) + offset);
}

constexpr std::uint16_t ChannelBlockCount(std::uint16_t channel_count) {
    return static_cast<std::uint16_t>(channel_count * kChannelStride);
}

// --- Scaling (register integer ↔ physical) -----------------------------------

constexpr double kTempScale = 10.0;
constexpr double kHumidityScale = 10.0;
constexpr double kSoilMoistureScale = 10.0;

constexpr std::int16_t EncodeTempC10(double celsius) {
    return static_cast<std::int16_t>(celsius * kTempScale);
}
constexpr double DecodeTempC(std::int16_t c10) {
    return static_cast<double>(c10) / kTempScale;
}

constexpr std::uint16_t EncodeHumidityPct10(double pct) {
    return static_cast<std::uint16_t>(pct * kHumidityScale);
}
constexpr double DecodeHumidityPct(std::uint16_t pct10) {
    return static_cast<double>(pct10) / kHumidityScale;
}

constexpr std::uint16_t EncodeSoilMoisturePct10(double pct) {
    return static_cast<std::uint16_t>(pct * kSoilMoistureScale);
}
constexpr double DecodeSoilMoisturePct(std::uint16_t pct10) {
    return static_cast<double>(pct10) / kSoilMoistureScale;
}

constexpr std::int16_t EncodeSoilTempC10(double celsius) {
    return EncodeTempC10(celsius);
}
constexpr double DecodeSoilTempC(std::int16_t c10) {
    return DecodeTempC(c10);
}

inline void PackUptimeS(std::uint32_t seconds,
                        std::uint16_t* lo,
                        std::uint16_t* hi) {
    *lo = static_cast<std::uint16_t>(seconds & 0xFFFFu);
    *hi = static_cast<std::uint16_t>((seconds >> 16) & 0xFFFFu);
}

inline std::uint32_t UnpackUptimeS(std::uint16_t lo, std::uint16_t hi) {
    return (static_cast<std::uint32_t>(hi) << 16) | lo;
}

inline void PackSeq(std::uint32_t seq, std::uint16_t* lo, std::uint16_t* hi) {
    PackUptimeS(seq, lo, hi);
}

inline std::uint32_t UnpackSeq(std::uint16_t lo, std::uint16_t hi) {
    return UnpackUptimeS(lo, hi);
}

// --- Topic sensor_id helper --------------------------------------------------

inline std::string MakeSensorId(const std::string& bus_id,
                                std::uint16_t unit_id,
                                std::uint16_t channel,
                                const std::string& quantity) {
    return bus_id + "/u" + std::to_string(unit_id) + "/ch" +
           std::to_string(channel) + "/" + quantity;
}

// Quantity name fragments used in MakeSensorId / docs.
constexpr const char* kQtyAirTemperature = "air_temperature";
constexpr const char* kQtyAirHumidity = "air_humidity";
constexpr const char* kQtyIlluminance = "illuminance";
constexpr const char* kQtySoilMoisture = "soil_moisture";
constexpr const char* kQtySoilTemperature = "soil_temperature";
constexpr const char* kQtyCo2 = "co2";

// --- Modbus session defaults (overridable in YAML) ---------------------------

constexpr int kDefaultRtuBaud = 9600;
constexpr int kDefaultRtuTimeoutMs = 200;
constexpr int kDefaultTcpPort = 502;
constexpr int kDefaultTcpMockPort = 1502;
constexpr int kDefaultTcpTimeoutMs = 500;
constexpr int kDefaultCommFailThreshold = 3;
constexpr int kDefaultPollPeriodMs = 500;

static_assert(ChannelBase(0) == 0x0100, "channel 0 base");
static_assert(ChannelBase(1) == 0x0110, "channel stride");
static_assert(ChannelReg(3, kChOffQuality) == 0x0136, "ch3 quality");
static_assert(ChannelBlockCount(10) == 160, "10-ch block words");
static_assert(DecodeTempC(EncodeTempC10(23.4)) > 23.39 &&
                  DecodeTempC(EncodeTempC10(23.4)) < 23.41,
              "temp scale round-trip");

}  // namespace afp
}  // namespace agri_demo
