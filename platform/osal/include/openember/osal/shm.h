/* OpenEmber OSAL — 共享内存（Linux: shm_open + mmap） */
#ifndef OPENEMBER_OSAL_SHM_H_
#define OPENEMBER_OSAL_SHM_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "openember/osal/types.h"

#include <stdint.h>
#include <stddef.h>

typedef struct oe_shm {
    uint8_t opaque[256];
} oe_shm_t;

typedef struct oe_shm_caps {
    uint32_t supports_shared_memory; /* 1 */
} oe_shm_caps_t;

oe_result_t oe_shm_query_caps(oe_shm_caps_t *out_caps);

/* name: shm_open name, typically starts with '/' (e.g. "/oe_shm_xxx") */
oe_result_t oe_shm_create(oe_shm_t *shm, const char *name, size_t size);
oe_result_t oe_shm_open(oe_shm_t *shm, const char *name);

/* Unlink removes the shared memory object name; mappings in other fds remain valid until unmapped. */
oe_result_t oe_shm_unlink(const char *name);

/* unmap + close (does not unlink) */
oe_result_t oe_shm_close(oe_shm_t *shm);

/* Get mapped pointer and size. Valid after create/open until close. */
oe_result_t oe_shm_get_ptr(oe_shm_t *shm, void **out_addr, size_t *out_size);

#ifdef __cplusplus
}
#endif

#endif /* OPENEMBER_OSAL_SHM_H_ */

