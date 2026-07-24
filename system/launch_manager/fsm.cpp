/*
 * Copyright (c) 2022-2023, OpenEmber Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2022-08-02     luhuadong    the first version
 */

#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define LOG_TAG "Workflow"
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

static const char *const g_StateText[EMBER_SYSTEM_COUNT] = {"Power Off", "Init", "Normal",
                                                           "Error", "Degrade", "Sleep"};

/*
 *    No.  |                      Action Functions
 * +-------+---------+------+-------+--------+---------+----------+----------+
 * | state | powerOn | init | sleep | wakeUp | goWrong | recovery | powerOff |
 * +-------+---------+------+-------+--------+---------+----------+----------+
 */
static State stateTable[EMBER_SYSTEM_COUNT] = {
    {EMBER_SYSTEM_POWEROFF, NULL, ignore, ignore, ignore, ignore, ignore, ignore}, /* 关闭状态 */
    {EMBER_SYSTEM_INIT, NULL, _init, ignore, ignore, ignore, ignore, _poweroff},   /* 初始状态 */
    {EMBER_SYSTEM_NORMAL, NULL, ignore, _sleep, ignore, _gowrong, ignore, _poweroff}, /* 正常状态 */
    {EMBER_SYSTEM_ERROR, NULL, ignore, ignore, ignore, ignore, _recovery, _poweroff}, /* 故障状态 */
    {EMBER_SYSTEM_DEGRADE, NULL, ignore, ignore, ignore, _gowrong, _recovery, _poweroff}, /* 降级状态 */
    {EMBER_SYSTEM_SLEEP, NULL, ignore, ignore, _wakeup, _gowrong, ignore, _poweroff} /* 休眠状态 */
};

static void ignore()
{
    /* Empty function, do nothing */
}

static void _init()
{
    LOG_I("LiDAR %s", g_StateText[pCurrentState->state]);
    pCurrentState = &stateTable[EMBER_SYSTEM_NORMAL];
    LOG_I(" --> %s", g_StateText[pCurrentState->state]);
}

static void _sleep()
{
    LOG_I("LiDAR %s", g_StateText[pCurrentState->state]);
    pCurrentState = &stateTable[EMBER_SYSTEM_SLEEP];
    LOG_I(" --> %s", g_StateText[pCurrentState->state]);
}

static void _wakeup()
{
    LOG_I("LiDAR %s", g_StateText[pCurrentState->state]);
    pCurrentState = &stateTable[EMBER_SYSTEM_INIT];
    LOG_I(" --> %s", g_StateText[pCurrentState->state]);
}

static void _gowrong()
{
    LOG_I("LiDAR %s", g_StateText[pCurrentState->state]);

    if (EMBER_SYSTEM_NORMAL == pCurrentState->state) {
        pCurrentState = &stateTable[EMBER_SYSTEM_DEGRADE];
    } else if (EMBER_SYSTEM_SLEEP == pCurrentState->state) {
        pCurrentState = &stateTable[EMBER_SYSTEM_ERROR];
    } else if (EMBER_SYSTEM_DEGRADE == pCurrentState->state) {
        pCurrentState = &stateTable[EMBER_SYSTEM_ERROR];
    }

    LOG_I(" --> %s", g_StateText[pCurrentState->state]);
}

static void _recovery()
{
    LOG_I("LiDAR %s", g_StateText[pCurrentState->state]);
    pCurrentState = &stateTable[EMBER_SYSTEM_INIT];
    LOG_I(" --> %s", g_StateText[pCurrentState->state]);
}

static void _poweroff()
{
    LOG_I("LiDAR %s", g_StateText[pCurrentState->state]);
    pCurrentState = &stateTable[EMBER_SYSTEM_POWEROFF];
    LOG_I(" --> %s", g_StateText[pCurrentState->state]);
}

static void onPowerOn()
{
    if (pCurrentState->powerOn) {
        pCurrentState->powerOn();
    }
    sem_post(&stateChangedSem);
}

static void onInit()
{
    if (pCurrentState->init) {
        pCurrentState->init();
    }
    sem_post(&stateChangedSem);
}

static void onSleep()
{
    if (pCurrentState->sleep) {
        pCurrentState->sleep();
    }
    sem_post(&stateChangedSem);
}

static void onWakeUp()
{
    if (pCurrentState->wakeUp) {
        pCurrentState->wakeUp();
    }
    sem_post(&stateChangedSem);
}

static void onGoWrong()
{
    if (pCurrentState->goWrong) {
        pCurrentState->goWrong();
    }
    sem_post(&stateChangedSem);
}

static void onRecovery()
{
    if (pCurrentState->recovery) {
        pCurrentState->recovery();
    }
    sem_post(&stateChangedSem);

    // if ok, then
    if (pCurrentState->init) {
        pCurrentState->init();
    }
    sem_post(&stateChangedSem);
}

static void onPowerOff()
{
    if (pCurrentState->powerOff) {
        pCurrentState->powerOff();
    }
    sem_post(&stateChangedSem);
}

State context = {EMBER_SYSTEM_INIT, onPowerOn, onInit, onSleep, onWakeUp, onGoWrong, onRecovery, onPowerOff};

int fsm_init(void)
{
    sem_init(&stateChangedSem, 0, 1);
    pCurrentState = &stateTable[EMBER_SYSTEM_INIT];

    return EMBER_EOK;
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
    return (char *)g_StateText[s];
}

