/* OpenEmber OSAL — 线程（Linux: pthread） */
#ifndef OPENEMBER_OSAL_THREAD_H_
#define OPENEMBER_OSAL_THREAD_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "openember/osal/types.h"

typedef void (*oe_thread_fn)(void *arg);

typedef struct oe_thread {
    unsigned char opaque[128];
} oe_thread_t;

/**
 * 创建线程并立即启动。线程入口返回后线程结束，需 oe_thread_join 回收资源。
 */
oe_result_t oe_thread_create(oe_thread_t *thread, oe_thread_fn fn, void *arg);

/**
 * 阻塞等待线程结束。同一线程仅能 join 一次。
 */
oe_result_t oe_thread_join(oe_thread_t *thread);

#ifdef __cplusplus
}
#endif

#endif /* OPENEMBER_OSAL_THREAD_H_ */
