/*
 * Copyright (c) 2022-2023, OpenEmber Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2022-08-02     luhuadong    the first version
 */

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <semaphore.h>

#define LOG_TAG                "Workflow"
#include "openember.h"
#include "fsm.h"

/*
 *  Message       Current State         Doing    >>   New State
 * +-------+     +--------------+     +--------+
 * | Event | --> | StateMachine | --> | Action |
 * +-------+     +--------------+     +--------+
 */

/* Action functions */
static void ignore();
static void _init();
static void _sleep();
static void _wakeup();
static void _gowrong();
static void _recovery();
static void _poweroff();

static State *pCurrentState;
static sem_t stateChangedSem;

static char *const g_StateText[AG_SYSTEM_COUNT] = {
    "Power Off", "Init", "Normal", "Error", "Degrade", "Sleep"
};

/*
 *    No.  |                      Action Functions
 * +-------+---------+------+-------+--------+---------+----------+----------+
 * | state | powerOn | init | sleep | wakeUp | goWrong | recovery | powerOff |
 * +-------+---------+------+-------+--------+---------+----------+----------+
 */
static State stateTable[AG_SYSTEM_COUNT] = {
    { AG_SYSTEM_POWEROFF, NULL, ignore, ignore, ignore,  ignore,   ignore,    ignore },     /* 关闭状态 */
    { AG_SYSTEM_INIT,     NULL, _init,  ignore, ignore,  ignore,   ignore,    _poweroff },  /* 初始状态 */
    { AG_SYSTEM_NORMAL,   NULL, ignore, _sleep, ignore,  _gowrong, ignore,    _poweroff },  /* 正常状态 */
    { AG_SYSTEM_ERROR,    NULL, ignore, ignore, ignore,  ignore,   _recovery, _poweroff },  /* 故障状态 */
    { AG_SYSTEM_DEGRADE,  NULL, ignore, ignore, ignore,  _gowrong, _recovery, _poweroff },  /* 降级状态 */
    { AG_SYSTEM_SLEEP,    NULL, ignore, ignore, _wakeup, _gowrong, ignore,    _poweroff }   /* 休眠状态 */
};

static void ignore()
{
    /* Empty function, do nothing */
}

static void _init()
{
    printf("LiDAR %s", g_StateText[pCurrentState->state]);
    pCurrentState = &stateTable[AG_SYSTEM_NORMAL];
    printf(" --> %s\n", g_StateText[pCurrentState->state]);
}

static void _sleep()
{
    printf("LiDAR %s", g_StateText[pCurrentState->state]);
    pCurrentState = &stateTable[AG_SYSTEM_SLEEP];
    printf(" --> %s\n", g_StateText[pCurrentState->state]);
}

static void _wakeup()
{
    printf("LiDAR %s", g_StateText[pCurrentState->state]);
    pCurrentState = &stateTable[AG_SYSTEM_INIT];
    printf(" --> %s\n", g_StateText[pCurrentState->state]);
}

static void _gowrong()
{
    printf("LiDAR %s", g_StateText[pCurrentState->state]);

    if (AG_SYSTEM_NORMAL == pCurrentState->state) {
        pCurrentState = &stateTable[AG_SYSTEM_DEGRADE];
    }
    else if (AG_SYSTEM_SLEEP == pCurrentState->state) {
        pCurrentState = &stateTable[AG_SYSTEM_ERROR];
    }
    else if (AG_SYSTEM_DEGRADE == pCurrentState->state) {
        pCurrentState = &stateTable[AG_SYSTEM_ERROR];
    }

    printf(" --> %s\n", g_StateText[pCurrentState->state]);
}

static void _recovery()
{
    printf("LiDAR %s", g_StateText[pCurrentState->state]);
    pCurrentState = &stateTable[AG_SYSTEM_INIT];
    printf(" --> %s\n", g_StateText[pCurrentState->state]);
}

static void _poweroff()
{
    printf("LiDAR %s", g_StateText[pCurrentState->state]);
    pCurrentState = &stateTable[AG_SYSTEM_POWEROFF];
    printf(" --> %s\n", g_StateText[pCurrentState->state]);
}

/* 
 */
static void onPowerOn(State *pThis)
{
    pCurrentState->powerOn(pThis);
    sem_post(&stateChangedSem);
}

static void onInit(State *pThis)
{
    pCurrentState->init(pThis);
    sem_post(&stateChangedSem);
}

static void onSleep(State *pThis)
{
    pCurrentState->sleep(pThis);
    sem_post(&stateChangedSem);
}

static void onWakeUp(State *pThis)
{
    pCurrentState->wakeUp(pThis);
    sem_post(&stateChangedSem);
}

static void onGoWrong(State *pThis)
{
    pCurrentState->goWrong(pThis);
    sem_post(&stateChangedSem);
}

static void onRecovery(State *pThis)
{
    pCurrentState->recovery(pThis);
    sem_post(&stateChangedSem);

    // if ok, then
    pCurrentState->init(pThis);
    sem_post(&stateChangedSem);
}

static void onPowerOff(State *pThis)
{
    pCurrentState->powerOff(pThis);
    sem_post(&stateChangedSem);
}

State context = {
    AG_SYSTEM_INIT,
    onPowerOn,
    onInit,
    onSleep,
    onWakeUp,
    onGoWrong,
    onRecovery,
    onPowerOff
};

int fsm_init(void)
{
    sem_init(&stateChangedSem, 0, 1);
    pCurrentState = &stateTable[AG_SYSTEM_INIT];

    return AG_EOK;
}

void fsm_deinit(void)
{
    sem_destroy(&stateChangedSem);
}

int fsm_sem_wait(void)
{
    return sem_wait(&stateChangedSem);
}

state_t fsm_get_current_state(void)
{
    return pCurrentState->state;
}

char *fsm_get_state_text(state_t s)
{
    return g_StateText[s];
}