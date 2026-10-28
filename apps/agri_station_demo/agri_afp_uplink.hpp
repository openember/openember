/*
 * Copyright (c) 2022-2026, OpenEmber Team
 * SPDX-License-Identifier: Apache-2.0
 *
 * OE-AFP wireless uplink PDU (node → gateway).
 * Design owner: docs/design/agri-fieldbus-protocol.md §7
 * Register semantics: agri_afp_regs.hpp
 */

#pragma once

#include <cstddef>
#include <cstdint>

#include "agri_afp_regs.hpp"

namespace agri_demo {
namespace afp {
namespace uplink {

constexpr std::uint16_t kMagic = 0xAE10;
constexpr std::uint8_t kVersion = 0x01;

enum class MsgType : std::uint8_t {
    kSample = 0x01,
    kHello = 0x02,
    kGoodbye = 0x03,
    kAck = 0x81,
};

constexpr std::uint8_t kFlagAckReq = 1u << 0;

// Fixed header before payload (little-endian on the wire).
constexpr std::size_t kHeaderSize = 12;
constexpr std::size_t kCrcSize = 2;

// Offsets within the 12-byte header.
constexpr std::size_t kOffMagic = 0;
constexpr std::size_t kOffVersion = 2;
constexpr std::size_t kOffMsgType = 3;
constexpr std::size_t kOffNodeId = 4;
constexpr std::size_t kOffChannel = 6;
constexpr std::size_t kOffFlags = 7;
constexpr std::size_t kOffSeq = 8;
constexpr std::size_t kOffPayloadLen = 10;

// SAMPLE payload (fixed layout, little-endian).
constexpr std::size_t kSamplePayloadSize = 20;
constexpr std::size_t kSampleOffPresentMask = 0;
constexpr std::size_t kSampleOffAirTempC10 = 2;
constexpr std::size_t kSampleOffAirHumidityPct10 = 4;
constexpr std::size_t kSampleOffIlluminanceLux = 6;
constexpr std::size_t kSampleOffSoilMoisturePct10 = 8;
constexpr std::size_t kSampleOffSoilTempC10 = 10;
constexpr std::size_t kSampleOffCo2Ppm = 12;
constexpr std::size_t kSampleOffQuality = 14;
constexpr std::size_t kSampleOffUnixS = 16;

// HELLO payload.
constexpr std::size_t kHelloPayloadSize = 4;
constexpr std::size_t kHelloOffCapsBits = 0;
constexpr std::size_t kHelloOffChannelCount = 2;

// ACK payload: echo seq (u16).
constexpr std::size_t kAckPayloadSize = 2;

// Demo default: UDP listen for gateway mock.
constexpr int kDefaultUdpPort = 15100;

// Modbus CRC-16 (poly 0xA001, init 0xFFFF) over magic..payload.
inline std::uint16_t Crc16Modbus(const std::uint8_t* data, std::size_t len) {
    std::uint16_t crc = 0xFFFF;
    for (std::size_t i = 0; i < len; ++i) {
        crc ^= data[i];
        for (int b = 0; b < 8; ++b) {
            if (crc & 1u) {
                crc = static_cast<std::uint16_t>((crc >> 1) ^ 0xA001u);
            } else {
                crc = static_cast<std::uint16_t>(crc >> 1);
            }
        }
    }
    return crc;
}

inline void WriteLe16(std::uint8_t* p, std::uint16_t v) {
    p[0] = static_cast<std::uint8_t>(v & 0xFFu);
    p[1] = static_cast<std::uint8_t>((v >> 8) & 0xFFu);
}

inline std::uint16_t ReadLe16(const std::uint8_t* p) {
    return static_cast<std::uint16_t>(p[0] | (static_cast<std::uint16_t>(p[1]) << 8));
}

inline void WriteLe32(std::uint8_t* p, std::uint32_t v) {
    WriteLe16(p, static_cast<std::uint16_t>(v & 0xFFFFu));
    WriteLe16(p + 2, static_cast<std::uint16_t>((v >> 16) & 0xFFFFu));
}

inline std::uint32_t ReadLe32(const std::uint8_t* p) {
    return UnpackUptimeS(ReadLe16(p), ReadLe16(p + 2));
}

}  // namespace uplink
}  // namespace afp
}  // namespace agri_demo
