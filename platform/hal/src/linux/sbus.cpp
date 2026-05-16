#define _GNU_SOURCE

#include "openember/hal/sbus.hpp"
#include "openember/hal/uart.hpp"
#include "openember/osal/time.hpp"

#include <cstring>

namespace openember::hal {

struct Sbus::Impl {
    Uart uart;
    bool inited = false;
    uint32_t baud_rate = 100000;
    uint8_t nonblocking = 0;
};

namespace {

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

Sbus::Sbus() : impl_(std::make_unique<Impl>()) {}

Sbus::~Sbus()
{
    (void)close();
}

Result Sbus::query_caps(SbusCaps* out_caps)
{
    if (!out_caps) {
        return osal::kErrInvalidArg;
    }

    out_caps->channels_count = 16;
    out_caps->switches_count = 2;
    out_caps->frame_len_bytes = 25;
    out_caps->baud_rate = 100000;
    return osal::kOk;
}

Result Sbus::open(const std::string& uart_path, const SbusConfig& cfg)
{
    if (uart_path.empty()) {
        return osal::kErrInvalidArg;
    }

    (void)close();

    impl_->baud_rate = (cfg.baud_rate != 0) ? cfg.baud_rate : 100000;
    impl_->nonblocking = (cfg.nonblocking != 0) ? 1 : 0;

    UartConfig ucfg {};
    ucfg.baud_rate = impl_->baud_rate;
    ucfg.data_bits = 8;
    ucfg.stop_bits = 2;
    ucfg.parity = UartParity::Even;
    ucfg.nonblocking = impl_->nonblocking;

    const Result r = impl_->uart.open(uart_path, ucfg);
    if (r != osal::kOk) {
        return r;
    }

    impl_->inited = true;
    return osal::kOk;
}

Result Sbus::close()
{
    (void)impl_->uart.close();
    impl_->inited = false;
    impl_->baud_rate = 100000;
    impl_->nonblocking = 0;
    return osal::kOk;
}

Result Sbus::recv_frame(SbusFrame* out_frame, int timeout_ms)
{
    if (!out_frame) {
        return osal::kErrInvalidArg;
    }

    if (!impl_->inited) {
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
        const Result r = impl_->uart.read(raw + got, want, &rd);

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
