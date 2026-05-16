#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

#include "openember/osal/types.hpp"

namespace openember::osal {

struct FifoCaps {
    uint32_t supports_fifo = 0;
};

enum class FifoMode : uint32_t {
    Read = 1u << 0,
    Write = 1u << 1,
    ReadWrite = Read | Write,
};

class Fifo {
public:
    Fifo();
    ~Fifo();

    Fifo(const Fifo&) = delete;
    Fifo& operator=(const Fifo&) = delete;
    Fifo(Fifo&&) = delete;
    Fifo& operator=(Fifo&&) = delete;

    static Result query_caps(FifoCaps* out_caps);
    static Result unlink(const std::string& path);

    Result open(const std::string& path, FifoMode mode);
    Result close();
    Result write(const void* buf, size_t len, size_t* out_written, int timeout_ms);
    Result read(void* buf, size_t len, size_t* out_read, int timeout_ms);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace openember::osal
