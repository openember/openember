#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace openember::sensor::detail {
struct YesenseFrame {
    std::uint16_t tid = 0;
    unsigned fields = 0; // accel=1, gyro=2, quaternion=4, temperature=8, magnetic SI=16
    double ax = 0, ay = 0, az = 0, wx = 0, wy = 0, wz = 0;
    double qw = 0, qx = 0, qy = 0, qz = 0, temperature = 0, mx = 0, my = 0, mz = 0;
};
struct YesenseReply {
    std::uint8_t category = 0, operation = 0;
    std::vector<std::uint8_t> payload;
};
// Both wire formats share 59 53. Consider checksum, length and payload structure,
// never identify a response solely by the low byte of a streaming TID.
class YesenseDecoder {
public:
    std::size_t Feed(const std::byte* data, std::size_t size, YesenseFrame& latest) {
        std::size_t decoded = 0;
        for (std::size_t i = 0; i < size; ++i) {
            buffer_.push_back(std::to_integer<std::uint8_t>(data[i]));
            while (buffer_.size() >= 2) {
                if (buffer_[0] != 0x59 || buffer_[1] != 0x53) { Drop(1); continue; }
                if (buffer_.size() < 5) break;
                const auto stream_size = std::size_t(7 + buffer_[4]);
                const unsigned word = buffer_[3] | (unsigned(buffer_[4]) << 8);
                const unsigned length = word >> 3, operation = word & 7;
                const bool reply_candidate = operation <= 2 && ReplyLength(buffer_[2], length);
                const auto reply_size = std::size_t(7 + length);
                YesenseFrame frame;
                if (buffer_.size() >= stream_size && Checksum(stream_size) && Decode(frame, stream_size - 2)) {
                    if (frame.fields) { latest = frame; ++decoded; }
                    Drop(stream_size); continue;
                }
                if (reply_candidate && buffer_.size() >= reply_size && Checksum(reply_size)) {
                    if (replies_.size() == 32) replies_.erase(replies_.begin());
                    replies_.push_back({buffer_[2], static_cast<std::uint8_t>(operation),
                        {buffer_.begin()+5, buffer_.begin()+reply_size-2}});
                    Drop(reply_size); continue;
                }
                if (buffer_.size() < stream_size || (reply_candidate && buffer_.size() < reply_size)) break;
                Drop(1);
            }
        }
        return decoded;
    }
    std::vector<YesenseReply> TakeReplies() {
        std::vector<YesenseReply> result; result.swap(replies_); return result;
    }
    static std::vector<std::byte> Command(std::uint8_t category, std::uint8_t operation,
                                        const std::vector<std::uint8_t>& payload) {
        const unsigned word = (unsigned(payload.size()) << 3) | operation;
        std::vector<std::byte> result{std::byte{0x59}, std::byte{0x53}, std::byte{category},
            std::byte(word & 255), std::byte(word >> 8)};
        for (auto value : payload) result.push_back(std::byte{value});
        std::uint8_t a = 0, b = 0;
        for (std::size_t i = 2; i < result.size(); ++i) { a += std::to_integer<std::uint8_t>(result[i]); b += a; }
        result.push_back(std::byte{a}); result.push_back(std::byte{b}); return result;
    }
private:
    static bool ReplyLength(unsigned category, unsigned length) {
        if (category == 0) return length >= 2 && length <= 64;
        if (category == 2 || category == 3) return length == 1;
        if (category == 4 || category == 0x4d) return length == 1 || length == 2;
        return false;
    }
    bool Checksum(std::size_t size) const {
        std::uint8_t a = 0, b = 0;
        for (std::size_t j = 2; j < size-2; ++j) { a += buffer_[j]; b += a; }
        return buffer_[size-2] == a && buffer_[size-1] == b;
    }
    bool Decode(YesenseFrame& f, std::size_t end) const {
        f.tid = buffer_[2] | (std::uint16_t(buffer_[3]) << 8);
        std::array<bool, 256> seen{};
        for (std::size_t j = 5; j < end;) {
            if (end-j < 2) return false;
            const auto id = buffer_[j++], length = buffer_[j++];
            if (length > end-j || seen[id]) return false;
            seen[id] = true;
            const unsigned expected = id == 1 ? 2 : id == 0x41 ? 16 :
                (id == 0x10 || id == 0x20 || id == 0x30 || id == 0x31 || id == 0x40) ? 12 :
                (id == 0x51 || id == 0x52) ? 4 : 0;
            if (expected && length != expected) return false;
            if (id == 0x10) { f.fields |= 1; f.ax=Value(j); f.ay=Value(j+4); f.az=Value(j+8); }
            else if (id == 0x20) {
                constexpr double radians = 0.017453292519943295;
                f.fields |= 2; f.wx=Value(j)*radians; f.wy=Value(j+4)*radians; f.wz=Value(j+8)*radians;
            } else if (id == 0x41) { f.fields |= 4; f.qw=Value(j); f.qx=Value(j+4); f.qy=Value(j+8); f.qz=Value(j+12); }
            else if (id == 1) {
                const unsigned bits = buffer_[j] | (unsigned(buffer_[j+1]) << 8);
                f.fields |= 8; f.temperature = (bits & 0x8000 ? int(bits)-65536 : int(bits)) * .01;
            } else if (id == 0x31) {
                // int32 * .001 mGauss = int32 * 1e-10 tesla.
                f.fields |= 16; f.mx=Value(j)*1e-4; f.my=Value(j+4)*1e-4; f.mz=Value(j+8)*1e-4;
            }
            // Normalized magnetic field (0x30) is dimensionless, not tesla.
            j += length;
        }
        return true;
    }
    double Value(std::size_t offset) const {
        const std::uint32_t bits = std::uint32_t(buffer_[offset]) | (std::uint32_t(buffer_[offset+1]) << 8) |
            (std::uint32_t(buffer_[offset+2]) << 16) | (std::uint32_t(buffer_[offset+3]) << 24);
        const std::int64_t value = bits & 0x80000000u ? std::int64_t(bits)-0x100000000LL : bits;
        return static_cast<double>(value)*1e-6;
    }
    void Drop(std::size_t n) { buffer_.erase(buffer_.begin(), buffer_.begin()+n); }
    std::vector<std::uint8_t> buffer_; // Both candidates bounded by 262 bytes.
    std::vector<YesenseReply> replies_; // Bounded even when no configuration client is active.
};
} // namespace openember::sensor::detail
