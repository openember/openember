#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

#include "openember/osal/types.hpp"

namespace openember::hal {

using Result = osal::Result;

enum class FileWhence {
    Set = 0,
    Cur = 1,
    End = 2,
};

enum class FileCapsFlag : uint32_t {
    CanRead = 1u << 0,
    CanWrite = 1u << 1,
    CanSeek = 1u << 2,
    CanFlush = 1u << 3,
};

struct FileCaps {
    uint32_t flags = 0;
    size_t max_read_bytes = 0;
    size_t max_write_bytes = 0;
};

class File {
public:
    File();
    ~File();

    File(const File&) = delete;
    File& operator=(const File&) = delete;
    File(File&&) = delete;
    File& operator=(File&&) = delete;

    static Result query_caps(FileCaps* out_caps);

    Result open(const std::string& path, const std::string& mode);
    Result close();
    Result read(void* buf, size_t len, size_t* out_read = nullptr);
    Result write(const void* buf, size_t len, size_t* out_written = nullptr);
    Result flush();
    Result seek(int64_t offset, FileWhence whence, int64_t* out_pos = nullptr);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace openember::hal
