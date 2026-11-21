#pragma once
#include <array>
#include <cstddef>
#include <cstdint>

namespace lpio {
// Linux termios2 snapshot, including custom BOTHER input/output baud rates.
// No kernel termios definitions leak into callers using libc termios.h.
class SerialSettings {
public:
    bool save(int fd) noexcept;
    bool restore(int fd) const noexcept;
private:
    alignas(std::max_align_t) std::array<unsigned char, 64> native_{};
    bool saved_ = false;
};
}
