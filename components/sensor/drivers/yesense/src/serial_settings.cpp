#include "serial_settings.hpp"
#include <asm/termbits.h>
#include <sys/ioctl.h>
#include <cstring>
#include <stdexcept>
namespace openember::sensor::detail {
bool SerialSettings::Save(int fd) noexcept {
    static_assert(sizeof(termios2) <= 64);
    return ioctl(fd,TCGETS2,native.data()) == 0;
}
bool SerialSettings::Restore(int fd) const noexcept { return ioctl(fd,TCSETS2,native.data()) == 0; }
std::uint32_t SerialSettings::BaudRate() const {
    termios2 settings{}; std::memcpy(&settings,native.data(),sizeof(settings));
    if (!settings.c_ospeed || settings.c_ispeed != settings.c_ospeed)
        throw std::runtime_error("different/zero host input-output baud rates are unsupported");
    return settings.c_ospeed;
}
}
