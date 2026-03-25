/* OpenEmber OSAL — 时间与睡眠（Linux: clock_gettime / nanosleep） */
#ifndef OPENEMBER_OSAL_TIME_H_
#define OPENEMBER_OSAL_TIME_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "openember/osal/types.h"

#include <stdint.h>

/** 睡眠至少 ms 毫秒（受调度影响） */
oe_result_t oe_sleep_ms(uint32_t ms);

/** 睡眠至少 ns 纳秒（会分片到 ms + 余量 ns） */
oe_result_t oe_sleep_ns(uint64_t ns);

/**
 * 单调时钟纳秒时间戳（自未指定起点递增，适用于超时与间隔测量）。
 * 成功时写入 *out_ns。
 */
oe_result_t oe_clock_monotonic_ns(uint64_t *out_ns);

#ifdef __cplusplus
}
#endif

#endif /* OPENEMBER_OSAL_TIME_H_ */
