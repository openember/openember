/* OpenEmber OSAL — 条件变量（Linux: pthread_cond） */
#ifndef OPENEMBER_OSAL_COND_H_
#define OPENEMBER_OSAL_COND_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "openember/osal/mutex.h"
#include "openember/osal/types.h"

#include <stdint.h>

typedef struct oe_cond {
    unsigned char opaque[128];
} oe_cond_t;

oe_result_t oe_cond_init(oe_cond_t *c);
oe_result_t oe_cond_destroy(oe_cond_t *c);

oe_result_t oe_cond_wait(oe_cond_t *c, oe_mutex_t *mutex);

/* timeout_ms <0: block forever; timeout_ms == 0: poll once; timeout_ms >0: up to N ms */
oe_result_t oe_cond_wait_timeout_ms(oe_cond_t *c, oe_mutex_t *mutex, int timeout_ms);

oe_result_t oe_cond_signal(oe_cond_t *c);
oe_result_t oe_cond_broadcast(oe_cond_t *c);

#ifdef __cplusplus
}
#endif

#endif /* OPENEMBER_OSAL_COND_H_ */

