#define _GNU_SOURCE

#include "openember/hal/sbus.hpp"

#include "openember/osal/time.hpp"

#include <cstring>

namespace openember::hal {

namespace {

constexpr uint32_t kSbusDefaultBaud = 100000;

Result decode_sbus_frame(const uint8_t raw[25], SbusFrame* out_frame)
{
    if (!raw || !out_frame) {
        return osal::kErrInvalidArg;
    }

    std::memset(out_frame, 0, sizeof(*out_frame));
    std::memcpy(out_frame->raw, raw, 25);

    for (int i = 0; i < 16; i++) {
        const int bit_index = i * 11;
        const int byte_index = 1 + (bit_index / 8);
        const int bit_offset = bit_index % 8;
        const uint16_t v = static_cast<uint16_t>(raw[byte_index] >> bit_offset) |
                           static_cast<uint16_t>(raw[byte_index + 1] << (8 - bit_offset));
        out_frame->channels[i] = static_cast<uint16_t>(v & 0x07FFu);
    }

    const uint8_t flags = raw[23];
    out_frame->frame_lost = (flags & 0x04u) ? 1u : 0u;
    out_frame->failsafe = (flags & 0x08u) ? 1u : 0u;
    out_frame->switches[0] = (flags & 0x10u) ? 1u : 0u;
    out_frame->switches[1] = (flags & 0x20u) ? 1u : 0u;
    return osal::kOk;
}

}  // namespace

Result Sbus::query_caps(SbusCaps* out_caps)
{
    if (!out_caps) {
        return osal::kErrInvalidArg;
    }

    out_caps->channels_count = 16;
    out_caps->switches_count = 2;
    out_caps->frame_len_bytes = 25;
    out_caps->baud_rate = kSbusDefaultBaud;
    return osal::kOk;
}

Result Sbus::open(const std::string& uart_path, const SbusConfig& cfg)
{
    if (uart_path.empty()) {
        return osal::kErrInvalidArg;
    }

    SerialPortConfig sp {};
    sp.baud_rate = (cfg.baud_rate != 0) ? cfg.baud_rate : kSbusDefaultBaud;
    sp.data_bits = 8;
    sp.stop_bits = 2;
    sp.parity = SerialParity::Even;
    sp.nonblocking = cfg.nonblocking;

    return SerialPort::open(uart_path, sp);
}

Result Sbus::recv_frame(SbusFrame* out_frame, int timeout_ms)
{
    if (!out_frame) {
        return osal::kErrInvalidArg;
    }

    if (!is_open()) {
        return osal::kErrInvalidArg;
    }

    uint8_t raw[25] {};
    size_t got = 0;

    uint64_t deadline_ns = 0;
    if (timeout_ms > 0) {
        uint64_t now_ns = 0;
        (void)osal::clock_monotonic_ns(&now_ns);
        deadline_ns = now_ns + static_cast<uint64_t>(timeout_ms) * 1000000ull;
    }

    while (got < sizeof(raw)) {
        const size_t want = sizeof(raw) - got;
        size_t rd = 0;
        const Result r = read(raw + got, want, &rd);

        if (r == osal::kOk) {
            if (rd == 0) {
                if (timeout_ms == 0) {
                    return osal::kErrTimeout;
                }
                if (timeout_ms > 0) {
                    uint64_t now_ns = 0;
                    (void)osal::clock_monotonic_ns(&now_ns);
                    if (now_ns >= deadline_ns) {
                        return osal::kErrTimeout;
                    }
                }
                (void)osal::sleep_ms(1);
                continue;
            }
            got += rd;
            continue;
        }

        if (r == osal::kErrAgain) {
            if (timeout_ms == 0) {
                return osal::kErrTimeout;
            }
            if (timeout_ms > 0) {
                uint64_t now_ns = 0;
                (void)osal::clock_monotonic_ns(&now_ns);
                if (now_ns >= deadline_ns) {
                    return osal::kErrTimeout;
                }
            }
            (void)osal::sleep_ms(1);
            continue;
        }

        return r;
    }

    if (raw[0] != 0x0Fu || raw[24] != 0x00u) {
        return osal::kErrInternal;
    }

    return decode_sbus_frame(raw, out_frame);
}

}  // namespace openember::hal
