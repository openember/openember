#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

#include "openember/osal/types.hpp"

namespace openember::osal {

struct ShmCaps {
    uint32_t supports_shared_memory = 0;
};

class SharedMemory {
public:
    SharedMemory();
    ~SharedMemory();

    SharedMemory(const SharedMemory&) = delete;
    SharedMemory& operator=(const SharedMemory&) = delete;
    SharedMemory(SharedMemory&&) = delete;
    SharedMemory& operator=(SharedMemory&&) = delete;

    static Result query_caps(ShmCaps* out_caps);
    static Result unlink(const std::string& name);

    Result create(const std::string& name, size_t size);
    Result open(const std::string& name);
    Result close();
    Result get_ptr(void** out_addr, size_t* out_size = nullptr);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace openember::osal
