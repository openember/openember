/*
 * Copyright (c) 2006-2020, DreamGrow Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2022-08-02     luhuadong    the first version
 */

#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include "agloo.h"
#include "message.h"

/*
 * 注册模块管理器
 *
 * int smm_register(smm_t *module, const char *name, const int pid, void *user_data);
 * int smm_unregister(smm_t *module);
 */

int msg_smm_register(msg_node_t handle, const char *name, mod_class_t cls)
{
    if (handle == NULL) {
        LOG_E("Message client is null, please check if connected");
        return -AG_ERROR;
    }

    smm_msg_t msg = {0};
    strncpy(msg.name, name, sizeof(msg.name)-1);
    msg.cls = cls;
    msg.pid = getpid();

    msg_bus_publish_raw(handle, MOD_REGISTER_TOPIC, (void *)&msg, sizeof(msg));

    return AG_EOK;
}

#if 0
int msg_smm_unregister(msg_node_t handle, const char *name, mod_class_t class)
{
    return AG_EOK;
}
#endif

/*
 * 定时发布心跳包
 */

int msg_keepalive_update(msg_node_t handle, const char *name, mod_class_t cls, state_t state)
{
    if (handle == NULL) {
        LOG_E("Message client is null, please check if connected");
        return -AG_ERROR;
    }
    
    keepalive_msg_t msg = {0};
    strncpy(msg.name, name, sizeof(msg.name)-1);
    msg.cls = cls;
    msg.state = state;

    msg_bus_publish_raw(handle, KEEPALIVE_TOPIC, (void *)&msg, sizeof(msg));

    return AG_EOK;
}