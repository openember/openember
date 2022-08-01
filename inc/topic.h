/*
 * Copyright (c) 2006-2020, DreamGrow Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2022-07-28     luhuadong    the first version
 */

#ifndef __AG_TOPIC_H__
#define __AG_TOPIC_H__

#include "agdef.h"

#define TEST_TOPIC                    "/test"

#define SYS_STATE_TOPIC               "/sys/state/post"
#define SYS_STATE_REPLY_TOPIC         "/sys/state/post_reply"
#define SYS_EVENT_TOPIC               "/sys/event/post"
#define SYS_EVENT_REPLY_TOPIC         "/sys/event/post_reply"
#define SYS_CONTROL_TOPIC             "/sys/control/post"
#define SYS_CONTROL_REPLY_TOPIC       "/sys/control/post_reply"
#define SYS_SERVICE_TOPIC             "/sys/service/post"
#define SYS_SERVICE_REPLY_TOPIC       "/sys/service/post_reply"

#define PROPERTY_POST_TOPIC           "/property/post"
#define PROPERTY_POST_REPLY_TOPIC     "/property/post_reply"

#define KEEPALIVE_POST_TOPIC          "/sys/keepalive"
#define KEEPALIVE_POST_REPLY_TOPIC    "/sys/keepalive_reply"

#define MOD_REGISTER_POST_TOPIC       "/sys/module/register"
#define MOD_REGISTER_POST_REPLY_TOPIC "/sys/module/register_reply"

/* Message packet definitions */
typedef struct state_msg
{
    state_t state_id;
    char    state_str[16];
} state_msg_t;

typedef struct event_msg
{
    const char *name;
    event_t event_id;
    union {
        char event_str[16];
        uint8_t err_code;
    } event_data;
} event_msg_t;

typedef struct keepalive_msg
{
    const char *name;
    mod_class_t class;
    state_t state;
    int timestamp;
} keepalive_msg_t;

typedef struct smm_register_msg
{
    const char *name;
    mod_class_t class;
    pid_t pid;
} smm_msg_t;


#ifdef __cplusplus
extern "C" {
#endif

#ifdef __cplusplus
}
#endif

#endif /* __AG_TOPIC_H__ */
