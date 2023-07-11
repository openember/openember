/*
 * Copyright (c) 2022-2023, OpenEmber Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2022-08-02     luhuadong    the first version
 */

#ifndef __FINITE_STATE_MACHINE_H__
#define __FINITE_STATE_MACHINE_H__

#include "agloo.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct State
{
    state_t state;
    void (*powerOn)();
    void (*init)();
    void (*sleep)();
    void (*wakeUp)();
    void (*goWrong)();
    void (*recovery)();
    void (*powerOff)();
} State;

//AG_EXT State context;

int fsm_init(void);
void fsm_deinit(void);

int fsm_sem_wait(void);
state_t fsm_get_current_state(void);
char *fsm_get_state_text(state_t s);

#ifdef __cplusplus
}
#endif

#endif /* __FINITE_STATE_MACHINE_H__ */