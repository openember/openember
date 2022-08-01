/*
 * Copyright (c) 2006-2020, DreamGrow Team
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

#ifdef __cplusplus
}
#endif

#endif /* __FINITE_STATE_MACHINE_H__ */