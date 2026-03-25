/* OpenEmber OSAL — 公共类型与错误码（C ABI） */
#ifndef OPENEMBER_OSAL_TYPES_H_
#define OPENEMBER_OSAL_TYPES_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/** 与 POSIX 习惯一致：0 表示成功，负值为错误 */
typedef int oe_result_t;

#define OE_OK 0

#define OE_ERR_INVALID_ARG (-1)
#define OE_ERR_NOMEM (-2)
#define OE_ERR_BUSY (-3)
#define OE_ERR_AGAIN (-4)
#define OE_ERR_INTERNAL (-5)
#define OE_ERR_UNSUPPORTED (-6)
#define OE_ERR_DEADLOCK (-7)
#define OE_ERR_TIMEOUT (-8)

#ifdef __cplusplus
}
#endif

#endif /* OPENEMBER_OSAL_TYPES_H_ */
