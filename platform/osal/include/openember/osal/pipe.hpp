#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>

#include "openember/osal/types.hpp"

namespace openember::osal {

struct PipeCaps {
    uint32_t supports_pipe = 0;
};

class Pipe {
public:
    Pipe();
    ~Pipe();

    Pipe(const Pipe&) = delete;
    Pipe& operator=(const Pipe&) = delete;
    Pipe(Pipe&&) = delete;
    Pipe& operator=(Pipe&&) = delete;

    static Result query_caps(PipeCaps* out_caps);

    Result create();
    Result close();
    Result write(const void* buf, size_t len, size_t* out_written, int timeout_ms);
    Result read(void* buf, size_t len, size_t* out_read, int timeout_ms);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace openember::osal
