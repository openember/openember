/*
 * Copyright (c) 2006-2020, DreamGrow Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2022-07-07     luhuadong    the first version
 * 2022-07-28     luhuadong    add message pub & sub
 * 2022-11-08     luhuadong    add multi-thread for message parse
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#define MODULE_NAME            "Template"
#define LOG_TAG                MODULE_NAME
#include "agloo.h"
#include "ppool.h"

#include "cJSON.h"

//#define TEMPLATE_RAW_MSG

static msg_node_t client;
static pool_t *ppool;

#ifdef TEMPLATE_RAW_MSG
typedef struct test_msg {
    int id;
    char *msg;
} test_msg_t;
#endif

static void thread_entry(void *args)
{
    if (!args) {
        return AG_EINVAL;
    }

    cJSON *json = cJSON_Parse(args);
    printf("%s\n", cJSON_Print(json));

    cJSON_Delete(json);
    free(args);
}

static void _msg_arrived_cb(char *topic, void *payload, size_t payloadlen)
{
#ifdef TEMPLATE_RAW_MSG
    test_msg_t *test_msg = (test_msg_t *)payload;
    printf("Payload len = %lu >> id: %d, msg: %s\n", payloadlen, test_msg->id, test_msg->msg);
#else
    LOG_D("[%s] %s", topic, (char *)payload);

    pool_task ptask;
    ptask.priority = 0;
    ptask.arg = strdup(payload);
    ptask.task = thread_entry;

    ppool_add(ppool, &ptask);
#endif
}

static int msg_init(void)
{
    int rc = 0, cn = 0;

    rc = msg_bus_init(&client, MODULE_NAME, NULL, _msg_arrived_cb);
    if (rc != AG_EOK) {
        LOG_E("Message bus init failed.\n");
        return -1;
    }

    ppool = ppool_init(5); /* create 5 thread */

    /* Subscription list */
    rc = msg_bus_subscribe(client, TEST_TOPIC);
    if (rc != AG_EOK) cn++;
    rc = msg_bus_subscribe(client, SYS_EVENT_REPLY_TOPIC);
    if (rc != AG_EOK) cn++;
    rc = msg_bus_subscribe(client, MOD_REGISTER_REPLY_TOPIC);
    if (rc != AG_EOK) cn++;

    if (cn != 0) {
        ppool_destroy(ppool);
        msg_bus_deinit(client);
        LOG_E("Message bus subscribe failed.\n");
        return -AG_ERROR;
    }

    return AG_EOK;
}

int main(void)
{
    int rc;

    sayHello(MODULE_NAME);
    
    log_init(MODULE_NAME);
    
    LOG_D("Hello Agloo!");
    LOG_I("Hello Agloo!");
    LOG_W("Hello Agloo!");
    LOG_E("Hello Agloo!");

    LOG_I("Version: %lu.%lu.%lu", AG_VERSION, AG_SUBVERSION, AG_REVISION);

    rc = msg_init();
    if (rc != AG_EOK) {
        LOG_E("Message channel init failed.");
        exit(1);
    }

    rc = msg_smm_register(client, MODULE_NAME, SUBMODULE_CLASS_TEST);
    if (rc != AG_EOK) {
        LOG_E("Module register failed.");
        exit(1);
    }

    int count = 3;
    
#ifdef TEMPLATE_RAW_MSG
    test_msg_t test_msg = {0};
#else
    char buf[256] = {0};
#endif

    while (count--) {
#ifdef TEMPLATE_RAW_MSG
    test_msg.id = count;
    test_msg.msg = "Hello, Agloo";
    msg_bus_publish_raw(client, TEST_TOPIC, &test_msg, sizeof(test_msg));
#else
        snprintf(buf, sizeof(buf), "{\"id\":\"%d\",\"msg\":\"Hello, Agloo\"}", count);
        msg_bus_publish(client, TEST_TOPIC, buf);
#endif
        sleep(1);
    }
    
    LOG_I("[Module] Template end.");
    msg_bus_deinit(client);
    log_deinit();

    return 0;
}