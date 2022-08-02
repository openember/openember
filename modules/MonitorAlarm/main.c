/*
 * Copyright (c) 2006-2020, DreamGrow Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2022-07-07     luhuadong    the first version
 */

#include <stdio.h>
#include <stdlib.h>
#include "agloo.h"

#define MODULE_NAME            "Monitor Alarm"

static msg_node_t client;

static void _msg_arrived_cb(char *topic, void *payload, size_t payloadlen)
{
    LOG_D("[%s] %s\n", topic, (char *)payload);
}

static int msg_init(void)
{
    int rc = 0, cn = 0;

    rc = msg_bus_init(&client, MODULE_NAME, NULL, _msg_arrived_cb);
    if (rc != AG_EOK) {
        printf("Message bus init failed.\n");
        return -1;
    }

    /* Subscription list */
    rc = msg_bus_subscribe(client, TEST_TOPIC);
    if (rc != AG_EOK) cn++;
    rc = msg_bus_subscribe(client, SYS_EVENT_REPLY_TOPIC);
    if (rc != AG_EOK) cn++;
    rc = msg_bus_subscribe(client, MOD_REGISTER_POST_REPLY_TOPIC);
    if (rc != AG_EOK) cn++;

    if (cn != 0) {
        msg_bus_deinit(client);
        printf("Message bus subscribe failed.\n");
        return -AG_ERROR;
    }

    return AG_EOK;
}

int main(void)
{
    int rc;

    sayHello(MODULE_NAME);
    
    log_init(MODULE_NAME);
    LOG_I("Version: %lu.%lu.%lu", AG_VERSION, AG_SUBVERSION, AG_REVISION);

    rc = msg_init();
    if (rc != AG_EOK) {
        LOG_E("Message channel init failed.");
        exit(1);
    }

    rc = msg_smm_register(client, MODULE_NAME, SUBMODULE_CLASS_MONITOR);
    if (rc != AG_EOK) {
        LOG_E("Module register failed.");
        exit(1);
    }

    return 0;
}