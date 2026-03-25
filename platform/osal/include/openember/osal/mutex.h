/* OpenEmber OSAL — 互斥锁（Linux: pthread_mutex） */
#ifndef OPENEMBER_OSAL_MUTEX_H_
#define OPENEMBER_OSAL_MUTEX_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "openember/osal/types.h"

/** 不透明大小：由实现填充，可静态分配（需容纳 pthread_mutex_t） */
typedef struct oe_mutex {
    unsigned char opaque[128];
} oe_mutex_t;

oe_result_t oe_mutex_init(oe_mutex_t *mutex);
oe_result_t oe_mutex_destroy(oe_mutex_t *mutex);
oe_result_t oe_mutex_lock(oe_mutex_t *mutex);
oe_result_t oe_mutex_trylock(oe_mutex_t *mutex);
oe_result_t oe_mutex_unlock(oe_mutex_t *mutex);

#ifdef __cplusplus
}
#endif

#endif /* OPENEMBER_OSAL_MUTEX_H_ */
