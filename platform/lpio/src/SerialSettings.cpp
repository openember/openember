#include "lpio/SerialSettings.hpp"
#include <asm/termbits.h>
#include <sys/ioctl.h>
namespace lpio {
bool SerialSettings::save(int fd) noexcept {
    static_assert(sizeof(termios2) <= 64);
    saved_ = ioctl(fd, TCGETS2, native_.data()) == 0;
    return saved_;
}
bool SerialSettings::restore(int fd) const noexcept {
    return saved_ && ioctl(fd, TCSETS2, native_.data()) == 0;
}
}
