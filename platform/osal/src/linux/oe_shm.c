#define _POSIX_C_SOURCE 200809L

#include "openember/osal/shm.h"

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

typedef struct {
    int fd;
    size_t size;
    void *addr;
    int inited;
} oe_shm_impl_t;

_Static_assert(sizeof(oe_shm_impl_t) <= sizeof(oe_shm_t), "oe_shm_t opaque too small (impl)");

static oe_shm_impl_t *impl(oe_shm_t *shm)
{
    return (oe_shm_impl_t *)(void *)shm;
}

oe_result_t oe_shm_query_caps(oe_shm_caps_t *out_caps)
{
    if (!out_caps) {
        return OE_ERR_INVALID_ARG;
    }
    out_caps->supports_shared_memory = 1;
    return OE_OK;
}

oe_result_t oe_shm_create(oe_shm_t *shm, const char *name, size_t size)
{
    int fd;
    struct stat st;
    oe_shm_impl_t *p;

    if (!shm || !name || size == 0) {
        return OE_ERR_INVALID_ARG;
    }

    memset(shm, 0, sizeof(*shm));
    p = impl(shm);
    p->fd = -1;
    p->addr = NULL;
    p->size = 0;
    p->inited = 0;

    fd = shm_open(name, O_CREAT | O_EXCL | O_RDWR, 0666);
    if (fd < 0) {
        if (errno == EEXIST) {
            return OE_ERR_BUSY;
        }
        return OE_ERR_INTERNAL;
    }

    if (ftruncate(fd, (off_t)size) != 0) {
        close(fd);
        shm_unlink(name);
        return OE_ERR_INTERNAL;
    }

    if (fstat(fd, &st) != 0) {
        close(fd);
        shm_unlink(name);
        return OE_ERR_INTERNAL;
    }

    p->size = (size_t)st.st_size;
    p->fd = fd;
    p->addr = mmap(NULL, p->size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (p->addr == MAP_FAILED) {
        p->addr = NULL;
        close(fd);
        shm_unlink(name);
        return OE_ERR_INTERNAL;
    }

    p->inited = 1;
    return OE_OK;
}

oe_result_t oe_shm_open(oe_shm_t *shm, const char *name)
{
    int fd;
    struct stat st;
    oe_shm_impl_t *p;

    if (!shm || !name) {
        return OE_ERR_INVALID_ARG;
    }

    memset(shm, 0, sizeof(*shm));
    p = impl(shm);
    p->fd = -1;
    p->addr = NULL;
    p->size = 0;
    p->inited = 0;

    fd = shm_open(name, O_RDWR, 0666);
    if (fd < 0) {
        return OE_ERR_UNSUPPORTED;
    }

    if (fstat(fd, &st) != 0) {
        close(fd);
        return OE_ERR_INTERNAL;
    }

    if (st.st_size == 0) {
        close(fd);
        return OE_ERR_INTERNAL;
    }

    p->size = (size_t)st.st_size;
    p->fd = fd;
    p->addr = mmap(NULL, p->size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (p->addr == MAP_FAILED) {
        p->addr = NULL;
        close(fd);
        return OE_ERR_INTERNAL;
    }

    p->inited = 1;
    return OE_OK;
}

oe_result_t oe_shm_unlink(const char *name)
{
    if (!name) {
        return OE_ERR_INVALID_ARG;
    }
    if (shm_unlink(name) != 0) {
        if (errno == ENOENT) {
            return OE_OK;
        }
        return OE_ERR_INTERNAL;
    }
    return OE_OK;
}

oe_result_t oe_shm_close(oe_shm_t *shm)
{
    oe_shm_impl_t *p;
    int fd;
    void *addr;
    size_t size;

    if (!shm) {
        return OE_ERR_INVALID_ARG;
    }

    p = impl(shm);
    fd = p->fd;
    addr = p->addr;
    size = p->size;

    memset(shm, 0, sizeof(*shm));

    if (addr && size > 0) {
        (void)munmap(addr, size);
    }
    if (fd >= 0) {
        (void)close(fd);
    }
    return OE_OK;
}

oe_result_t oe_shm_get_ptr(oe_shm_t *shm, void **out_addr, size_t *out_size)
{
    oe_shm_impl_t *p;

    if (!shm || !out_addr) {
        return OE_ERR_INVALID_ARG;
    }

    p = impl(shm);
    if (!p->inited || p->addr == NULL || p->size == 0) {
        return OE_ERR_INVALID_ARG;
    }

    *out_addr = p->addr;
    if (out_size) {
        *out_size = p->size;
    }
    return OE_OK;
}

