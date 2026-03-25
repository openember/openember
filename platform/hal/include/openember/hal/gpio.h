/* OpenEmber HAL — Device: GPIO (C ABI, Linux sysfs) */
#ifndef OPENEMBER_HAL_GPIO_H_
#define OPENEMBER_HAL_GPIO_H_

#include "openember/osal/types.h"

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct oe_gpio {
    uint8_t opaque[128];
} oe_gpio_t;

typedef enum oe_gpio_direction {
    OE_GPIO_DIR_IN = 0,
    OE_GPIO_DIR_OUT = 1,
} oe_gpio_direction_t;

typedef enum oe_gpio_caps_flag {
    OE_GPIO_CAPS_DIR_IN = 1u << 0,
    OE_GPIO_CAPS_DIR_OUT = 1u << 1,
} oe_gpio_caps_flag_t;

typedef struct oe_gpio_caps {
    uint32_t direction_mask; /* oe_gpio_caps_flag_t bitmask */
} oe_gpio_caps_t;

oe_result_t oe_gpio_query_caps(oe_gpio_caps_t *out_caps);

/* Open by Linux GPIO number (sysfs gpio<N>) */
oe_result_t oe_gpio_open(oe_gpio_t *g, uint32_t gpio_num);
oe_result_t oe_gpio_set_direction(oe_gpio_t *g, oe_gpio_direction_t dir);

/* value: 0 or 1 */
oe_result_t oe_gpio_read(oe_gpio_t *g, uint8_t *out_value);
oe_result_t oe_gpio_write(oe_gpio_t *g, uint8_t value);

oe_result_t oe_gpio_close(oe_gpio_t *g);

#ifdef __cplusplus
}
#endif

#endif /* OPENEMBER_HAL_GPIO_H_ */

