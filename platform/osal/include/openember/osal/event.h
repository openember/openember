/* OpenEmber OSAL — 事件（auto-reset/notify-one 语义，Linux: pthread cond+mutex） */
#ifndef OPENEMBER_OSAL_EVENT_H_
#define OPENEMBER_OSAL_EVENT_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "openember/osal/types.h"

#include <stdint.h>

typedef struct oe_event {
    unsigned char opaque[128];
} oe_event_t;

oe_result_t oe_event_init(oe_event_t *e);
oe_result_t oe_event_destroy(oe_event_t *e);

/* Set event signaled; wakes at most one waiter; auto-reset after a successful wait. */
oe_result_t oe_event_set(oe_event_t *e);

/* Reset to non-signaled. */
oe_result_t oe_event_reset(oe_event_t *e);

/* Wait until signaled, then auto-reset. */
oe_result_t oe_event_wait(oe_event_t *e);

/* timeout_ms <0: block forever; timeout_ms == 0: poll once; timeout_ms >0: up to N ms */
oe_result_t oe_event_wait_timeout_ms(oe_event_t *e, int timeout_ms);

#ifdef __cplusplus
}
#endif

#endif /* OPENEMBER_OSAL_EVENT_H_ */

