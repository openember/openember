/* OpenEmber OSAL — 信号量（Linux: sem_t） */
#ifndef OPENEMBER_OSAL_SEM_H_
#define OPENEMBER_OSAL_SEM_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "openember/osal/types.h"

#include <stdint.h>

typedef struct oe_sem {
    unsigned char opaque[128];
} oe_sem_t;

oe_result_t oe_sem_init(oe_sem_t *s, uint32_t initial_count);
oe_result_t oe_sem_destroy(oe_sem_t *s);

oe_result_t oe_sem_post(oe_sem_t *s);

/* Wait decrements the semaphore, blocks if count is 0. */
oe_result_t oe_sem_wait(oe_sem_t *s);

/* Return OE_ERR_AGAIN if not available. */
oe_result_t oe_sem_trywait(oe_sem_t *s);

/* timeout_ms <0: block forever; timeout_ms == 0: poll once; timeout_ms >0: up to N ms */
oe_result_t oe_sem_wait_timeout_ms(oe_sem_t *s, int timeout_ms);

#ifdef __cplusplus
}
#endif

#endif /* OPENEMBER_OSAL_SEM_H_ */

