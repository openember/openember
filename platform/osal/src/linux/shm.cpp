#define _POSIX_C_SOURCE 200809L

#include "openember/osal/shm.hpp"

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>

namespace openember::osal {

struct SharedMemory::Impl {
    int fd = -1;
    size_t size = 0;
    void* addr = nullptr;
    bool inited = false;
};

SharedMemory::SharedMemory() : impl_(std::make_unique<Impl>()) {}

SharedMemory::~SharedMemory()
{
    (void)close();
}

Result SharedMemory::query_caps(ShmCaps* out_caps)
{
    if (!out_caps) {
        return kErrInvalidArg;
    }
    out_caps->supports_shared_memory = 1;
    return kOk;
}

Result SharedMemory::unlink(const std::string& name)
{
    if (name.empty()) {
        return kErrInvalidArg;
    }
    if (::shm_unlink(name.c_str()) != 0) {
        return errno == ENOENT ? kOk : kErrInternal;
    }
    return kOk;
}

Result SharedMemory::create(const std::string& name, size_t size)
{
    if (name.empty() || size == 0) {
        return kErrInvalidArg;
    }

    impl_ = std::make_unique<Impl>();
    const int fd = shm_open(name.c_str(), O_CREAT | O_EXCL | O_RDWR, 0666);
    if (fd < 0) {
        return errno == EEXIST ? kErrBusy : kErrInternal;
    }

    if (ftruncate(fd, static_cast<off_t>(size)) != 0) {
        ::close(fd);
        ::shm_unlink(name.c_str());
        return kErrInternal;
    }

    struct stat st {};
    if (fstat(fd, &st) != 0) {
        ::close(fd);
        ::shm_unlink(name.c_str());
        return kErrInternal;
    }

    void* addr = mmap(nullptr, static_cast<size_t>(st.st_size), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (addr == MAP_FAILED) {
        ::close(fd);
        ::shm_unlink(name.c_str());
        return kErrInternal;
    }

    impl_->fd = fd;
    impl_->size = static_cast<size_t>(st.st_size);
    impl_->addr = addr;
    impl_->inited = true;
    return kOk;
}

Result SharedMemory::open(const std::string& name)
{
    if (name.empty()) {
        return kErrInvalidArg;
    }

    impl_ = std::make_unique<Impl>();
    const int fd = shm_open(name.c_str(), O_RDWR, 0666);
    if (fd < 0) {
        return kErrUnsupported;
    }

    struct stat st {};
    if (fstat(fd, &st) != 0 || st.st_size == 0) {
        ::close(fd);
        return kErrInternal;
    }

    void* addr = mmap(nullptr, static_cast<size_t>(st.st_size), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (addr == MAP_FAILED) {
        ::close(fd);
        return kErrInternal;
    }

    impl_->fd = fd;
    impl_->size = static_cast<size_t>(st.st_size);
    impl_->addr = addr;
    impl_->inited = true;
    return kOk;
}

Result SharedMemory::close()
{
    if (!impl_) {
        return kErrInvalidArg;
    }

    const int fd = impl_->fd;
    void* addr = impl_->addr;
    const size_t size = impl_->size;
    impl_ = std::make_unique<Impl>();

    if (addr && size > 0) {
        munmap(addr, size);
    }
    if (fd >= 0) {
        ::close(fd);
    }
    return kOk;
}

Result SharedMemory::get_ptr(void** out_addr, size_t* out_size)
{
    if (!impl_ || !out_addr) {
        return kErrInvalidArg;
    }
    if (!impl_->inited || impl_->addr == nullptr || impl_->size == 0) {
        return kErrInvalidArg;
    }
    *out_addr = impl_->addr;
    if (out_size) {
        *out_size = impl_->size;
    }
    return kOk;
}

}  // namespace openember::osal
