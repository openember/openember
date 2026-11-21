#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
namespace openember::sensor::detail {
// Preserve Linux termios2, including numeric BOTHER speeds, without exposing
// kernel termios definitions alongside libc termios headers in the driver TU.
struct SerialSettings {
    alignas(std::max_align_t) std::array<unsigned char,64> native{};
    bool Save(int fd) noexcept;
    bool Restore(int fd) const noexcept;
    std::uint32_t BaudRate() const;
};
}
