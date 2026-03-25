/* OpenEmber HAL — C++ wrapper: File (RAII)
 *
 * This header provides a thin C++ layer on top of the C ABI in file.h.
 */
#ifndef OPENEMBER_HAL_FILE_HPP_
#define OPENEMBER_HAL_FILE_HPP_

#include "openember/hal/file.h"

#include <string>

namespace oe {
namespace hal {

class File {
public:
    File() = default;

    File(const std::string &path, const char *mode)
    {
        (void)open(path, mode);
    }

    ~File()
    {
        /* Safe even when not opened; C side checks init state. */
        (void)oe_file_close(&f_);
    }

    File(const File &) = delete;
    File &operator=(const File &) = delete;

    File(File &&other) noexcept
    {
        f_ = other.f_;
        opened_ = other.opened_;
        other.opened_ = false;
    }

    File &operator=(File &&other) noexcept
    {
        if (this != &other) {
            (void)oe_file_close(&f_);
            f_ = other.f_;
            opened_ = other.opened_;
            other.opened_ = false;
        }
        return *this;
    }

    oe_result_t open(const std::string &path, const char *mode)
    {
        oe_result_t r = oe_file_open(&f_, path.c_str(), mode);
        opened_ = (r == OE_OK);
        return r;
    }

    oe_result_t close()
    {
        opened_ = false;
        return oe_file_close(&f_);
    }

    oe_result_t read(void *buf, size_t len, size_t *out_read = nullptr)
    {
        return oe_file_read(&f_, buf, len, out_read);
    }

    oe_result_t write(const void *buf, size_t len, size_t *out_written = nullptr)
    {
        return oe_file_write(&f_, buf, len, out_written);
    }

    oe_result_t flush()
    {
        return oe_file_flush(&f_);
    }

    oe_result_t seek(int64_t offset, oe_file_whence_t whence = OE_FILE_SEEK_SET, int64_t *out_pos = nullptr)
    {
        return oe_file_seek(&f_, offset, whence, out_pos);
    }

    oe_result_t query_caps(oe_file_caps_t *out_caps) const
    {
        return oe_file_query_caps(out_caps);
    }

private:
    oe_file_t f_ {};
    bool opened_ = false;
};

} // namespace hal
} // namespace oe

#endif /* OPENEMBER_HAL_FILE_HPP_ */

