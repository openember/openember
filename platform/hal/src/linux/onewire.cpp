#define _GNU_SOURCE

#include "openember/hal/onewire.hpp"

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>

namespace openember::hal {

struct Onewire::Impl {
    std::string w1_slave_path;
    bool inited = false;
};

namespace {

Result read_all_text(const char* path, char* out_buf, size_t cap, size_t* out_len)
{
    if (!path || !out_buf || cap == 0) {
        return osal::kErrInvalidArg;
    }

    const int fd = ::open(path, O_RDONLY);
    if (fd < 0) {
        return osal::kErrUnsupported;
    }

    size_t total = 0;
    while (total + 1 < cap) {
        const ssize_t n = ::read(fd, out_buf + total, cap - 1 - total);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            ::close(fd);
            return osal::kErrInternal;
        }
        if (n == 0) {
            break;
        }
        total += static_cast<size_t>(n);
    }

    out_buf[total] = '\0';
    if (out_len) {
        *out_len = total;
    }
    ::close(fd);
    return osal::kOk;
}

Result parse_temperature(const char* text, OnewireTemp* out_temp)
{
    if (!text || !out_temp) {
        return osal::kErrInvalidArg;
    }

    std::memset(out_temp, 0, sizeof(*out_temp));

    const char* yes_pos = std::strstr(text, "YES");
    const char* no_pos = std::strstr(text, "NO");
    if (yes_pos && (!no_pos || yes_pos < no_pos)) {
        out_temp->crc_ok = 1;
    }

    const char* t_pos = std::strstr(text, "t=");
    if (!t_pos) {
        return osal::kErrInternal;
    }

    t_pos += 2;
    char* endptr = nullptr;
    const long t_milli = std::strtol(t_pos, &endptr, 10);
    if (endptr == t_pos) {
        return osal::kErrInternal;
    }

    out_temp->temperature_milli_c = static_cast<int32_t>(t_milli);
    out_temp->temperature_c = static_cast<float>(t_milli) / 1000.0f;
    return osal::kOk;
}

}  // namespace

Onewire::Onewire() : impl_(std::make_unique<Impl>()) {}

Onewire::~Onewire()
{
    (void)close();
}

Result Onewire::query_caps(OnewireCaps* out_caps)
{
    if (!out_caps) {
        return osal::kErrInvalidArg;
    }

    out_caps->supports_temperature = 1;
    out_caps->supports_raw_read = 1;
    return osal::kOk;
}

Result Onewire::open(const std::string& w1_slave_path)
{
    if (w1_slave_path.empty()) {
        return osal::kErrInvalidArg;
    }

    (void)close();

    impl_->w1_slave_path = w1_slave_path;
    impl_->inited = true;

    if (access(impl_->w1_slave_path.c_str(), R_OK) != 0) {
        impl_->inited = false;
        impl_->w1_slave_path.clear();
        return osal::kErrUnsupported;
    }
    return osal::kOk;
}

Result Onewire::close()
{
    impl_->w1_slave_path.clear();
    impl_->inited = false;
    return osal::kOk;
}

Result Onewire::read_raw(char* out_buf, size_t cap, size_t* out_len)
{
    if (!out_buf || cap == 0) {
        return osal::kErrInvalidArg;
    }

    if (!impl_->inited || impl_->w1_slave_path.empty()) {
        return osal::kErrInvalidArg;
    }

    return read_all_text(impl_->w1_slave_path.c_str(), out_buf, cap, out_len);
}

Result Onewire::read_temperature(OnewireTemp* out_temp)
{
    if (!out_temp) {
        return osal::kErrInvalidArg;
    }

    char buf[256];
    size_t len = 0;
    const Result r = read_raw(buf, sizeof(buf), &len);
    if (r != osal::kOk) {
        return r;
    }

    return parse_temperature(buf, out_temp);
}

}  // namespace openember::hal
