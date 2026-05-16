#define _POSIX_C_SOURCE 200809L

#include "openember/hal/file.hpp"

#include <cstdio>
#include <cstring>

namespace openember::hal {

struct File::Impl {
    FILE* fp = nullptr;
    bool inited = false;
};

File::File() : impl_(std::make_unique<Impl>()) {}

File::~File()
{
    (void)close();
}

Result File::query_caps(FileCaps* out_caps)
{
    if (!out_caps) {
        return osal::kErrInvalidArg;
    }

    out_caps->flags = static_cast<uint32_t>(FileCapsFlag::CanRead) |
                      static_cast<uint32_t>(FileCapsFlag::CanWrite) |
                      static_cast<uint32_t>(FileCapsFlag::CanSeek) |
                      static_cast<uint32_t>(FileCapsFlag::CanFlush);
    out_caps->max_read_bytes = 0;
    out_caps->max_write_bytes = 0;
    return osal::kOk;
}

Result File::open(const std::string& path, const std::string& mode)
{
    if (path.empty() || mode.empty()) {
        return osal::kErrInvalidArg;
    }

    (void)close();

    FILE* fp = std::fopen(path.c_str(), mode.c_str());
    if (!fp) {
        return osal::kErrInternal;
    }

    impl_->fp = fp;
    impl_->inited = true;
    return osal::kOk;
}

Result File::close()
{
    if (!impl_->inited || !impl_->fp) {
        impl_->fp = nullptr;
        impl_->inited = false;
        return osal::kOk;
    }

    const int r = std::fclose(impl_->fp);
    impl_->fp = nullptr;
    impl_->inited = false;
    return r == 0 ? osal::kOk : osal::kErrInternal;
}

Result File::read(void* buf, size_t len, size_t* out_read)
{
    if (!buf && len > 0) {
        return osal::kErrInvalidArg;
    }

    if (!impl_->inited || !impl_->fp) {
        return osal::kErrInvalidArg;
    }

    if (len == 0) {
        if (out_read) {
            *out_read = 0;
        }
        return osal::kOk;
    }

    const size_t n = std::fread(buf, 1, len, impl_->fp);
    if (out_read) {
        *out_read = n;
    }
    if (n == 0 && std::ferror(impl_->fp)) {
        return osal::kErrInternal;
    }
    return osal::kOk;
}

Result File::write(const void* buf, size_t len, size_t* out_written)
{
    if (!buf && len > 0) {
        return osal::kErrInvalidArg;
    }

    if (!impl_->inited || !impl_->fp) {
        return osal::kErrInvalidArg;
    }

    if (len == 0) {
        if (out_written) {
            *out_written = 0;
        }
        return osal::kOk;
    }

    const size_t n = std::fwrite(buf, 1, len, impl_->fp);
    if (out_written) {
        *out_written = n;
    }
    if (n != len) {
        return osal::kErrInternal;
    }
    return osal::kOk;
}

Result File::flush()
{
    if (!impl_->inited || !impl_->fp) {
        return osal::kErrInvalidArg;
    }

    return std::fflush(impl_->fp) == 0 ? osal::kOk : osal::kErrInternal;
}

Result File::seek(int64_t offset, FileWhence whence, int64_t* out_pos)
{
    if (!impl_->inited || !impl_->fp) {
        return osal::kErrInvalidArg;
    }

    int w = 0;
    switch (whence) {
    case FileWhence::Set:
        w = SEEK_SET;
        break;
    case FileWhence::Cur:
        w = SEEK_CUR;
        break;
    case FileWhence::End:
        w = SEEK_END;
        break;
    default:
        return osal::kErrInvalidArg;
    }

    if (fseeko(impl_->fp, static_cast<off_t>(offset), w) != 0) {
        return osal::kErrInternal;
    }

    const off_t pos = ftello(impl_->fp);
    if (pos == static_cast<off_t>(-1)) {
        return osal::kErrInternal;
    }

    if (out_pos) {
        *out_pos = static_cast<int64_t>(pos);
    }
    return osal::kOk;
}

}  // namespace openember::hal
