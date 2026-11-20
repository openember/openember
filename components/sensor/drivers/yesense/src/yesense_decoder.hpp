#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

namespace openember::sensor::detail {
struct YesenseFrame {
    std::uint16_t tid = 0;
    double ax = 0, ay = 0, az = 0;
    double wx = 0, wy = 0, wz = 0;
    double qw = 0, qx = 0, qy = 0, qz = 0;
};

// YESENSE streaming data: 59 53, LE16 TID, U8 payload length, TLVs,
// CK1/CK2 (Fletcher sums over TID through payload). No device commands.
class YesenseDecoder {
public:
    std::size_t Feed(const std::byte* data, std::size_t size, YesenseFrame& latest) {
        std::size_t decoded = 0;
        for (std::size_t i = 0; i < size; ++i) {
            buffer_.push_back(std::to_integer<std::uint8_t>(data[i]));
            while (buffer_.size() >= 2) {
                if (buffer_[0] != 0x59 || buffer_[1] != 0x53) {
                    buffer_.erase(buffer_.begin());
                    continue;
                }
                if (buffer_.size() < 5) break;
                const std::size_t end = 5 + buffer_[4];
                if (buffer_.size() < end + 2) break;
                std::uint8_t ck1 = 0, ck2 = 0;
                for (std::size_t j = 2; j < end; ++j) {
                    ck1 += buffer_[j];
                    ck2 += ck1;
                }
                if (buffer_[end] != ck1 || buffer_[end + 1] != ck2) {
                    buffer_.erase(buffer_.begin());
                    continue;
                }
                YesenseFrame frame;
                frame.tid = buffer_[2] | (std::uint16_t(buffer_[3]) << 8);
                unsigned fields = 0;
                bool valid = true;
                for (std::size_t j = 5; j < end;) {
                    if (end - j < 2) { valid = false; break; }
                    const auto id = buffer_[j++];
                    const auto length = buffer_[j++];
                    if (length > end - j) { valid = false; break; }
                    unsigned field = 0;
                    if (id == 0x10 || id == 0x20 || id == 0x41) {
                        field = id == 0x10 ? 1 : id == 0x20 ? 2 : 4;
                        if ((fields & field) || length != (id == 0x41 ? 16 : 12)) {
                            valid = false; break;
                        }
                        fields |= field;
                        if (id == 0x10) {
                            frame.ax = Value(j); frame.ay = Value(j+4); frame.az = Value(j+8);
                        } else if (id == 0x20) {
                            constexpr double radians = 0.017453292519943295;
                            frame.wx = Value(j)*radians; frame.wy = Value(j+4)*radians; frame.wz = Value(j+8)*radians;
                        } else {
                            frame.qw = Value(j); frame.qx = Value(j+4); frame.qy = Value(j+8); frame.qz = Value(j+12);
                        }
                    }
                    j += length; // Unknown TLVs are safely skipped; never merged across frames.
                }
                if (valid && fields == 7) { latest = frame; ++decoded; }
                buffer_.erase(buffer_.begin(), buffer_.begin() + end + 2);
            }
        }
        return decoded;
    }
private:
    double Value(std::size_t offset) const {
        const std::uint32_t bits = std::uint32_t(buffer_[offset]) |
            (std::uint32_t(buffer_[offset+1]) << 8) |
            (std::uint32_t(buffer_[offset+2]) << 16) |
            (std::uint32_t(buffer_[offset+3]) << 24);
        // Arithmetic conversion avoids implementation-defined unsigned-to-signed casts.
        const std::int64_t signed_value = bits & 0x80000000u ? std::int64_t(bits) - 0x100000000LL : bits;
        return static_cast<double>(signed_value) * 1e-6;
    }
    std::vector<std::uint8_t> buffer_; // At most 262 bytes, including partial/noisy frames.
};
} // namespace openember::sensor::detail
