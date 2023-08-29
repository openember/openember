/*
 * Copyright (c) 2022-2023, OpenEmber Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2022-07-07     luhuadong    the first version
 * 2022-07-28     luhuadong    add message pub & sub
 * 2022-11-08     luhuadong    add multi-thread for message parse
 * 2022-11-08     luhuadong    support synchronous mode
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#define MODULE_NAME            "Template"
#define LOG_TAG                MODULE_NAME
#include "openember.h"
#include "ppool.h"

#include "cJSON.h"
#include "yaml.h"

//#define TEMPLATE_RAW_MSG

static msg_node_t client;
static pool_t *ppool;

/* Options */
static ag_bool_t sync_mode = AG_FALSE; /* AG_TRUE or AG_FALSE */

#ifdef TEMPLATE_RAW_MSG
typedef struct test_msg {
    int id;
    char *msg;
} test_msg_t;
#endif

static void task_entry(void *args)
{
    if (!args) {
        LOG_E("Invalid arguments for thread routine");
        return ;
    }

    cJSON *json = cJSON_Parse((char *)args);
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

    pool_task ptask = {
        .entry     = task_entry,
        .parameter = strdup((char *)payload),
        .priority  = 0,
    };

    ppool_add(ppool, &ptask);
#endif
}

static int msg_init(void)
{
    int rc = 0, cn = 0;

    if (sync_mode) {
        rc = msg_bus_init(&client, MODULE_NAME, NULL, NULL);
    }
    else {
        rc = msg_bus_init(&client, MODULE_NAME, NULL, _msg_arrived_cb);
    }
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

void yaml_test(void)
{
    int major = -1;
    int minor = -1;
    int patch = -1;
    char buf[64];

    yaml_get_version(&major, &minor, &patch);
    sprintf(buf, "%d.%d.%d", major, minor, patch);
    assert(strcmp(buf, yaml_get_version_string()) == 0);
    
    LOG_I("libyaml version %s", buf);

    /* Print structure sizes. */
    printf("sizeof(token) = %ld\n", (long)sizeof(yaml_token_t));
    printf("sizeof(event) = %ld\n", (long)sizeof(yaml_event_t));
    printf("sizeof(parser) = %ld\n", (long)sizeof(yaml_parser_t));
}

int main(void)
{
    int rc;

    sayHello(MODULE_NAME);
    
    log_init(MODULE_NAME);
    
    LOG_D("Hello OpenEmber!");
    LOG_I("Hello OpenEmber!");
    LOG_W("Hello OpenEmber!");
    LOG_E("Hello OpenEmber!");

    LOG_I("Version: %lu.%lu.%lu", AG_VERSION, AG_SUBVERSION, AG_REVISION);

    yaml_test();

    rc = msg_init();
    if (rc != AG_EOK) {
        LOG_E("Message channel init failed.");
        exit(1);
    }

    rc = msg_smm_register(client, MODULE_NAME, SUBmod_class_tEST);
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
        test_msg.msg = "Hello, OpenEmber";
        msg_bus_publish_raw(client, TEST_TOPIC, &test_msg, sizeof(test_msg));
#else
        snprintf(buf, sizeof(buf), "{\"id\":\"%d\",\"msg\":\"Hello, OpenEmber\"}", count);
        msg_bus_publish(client, TEST_TOPIC, buf);

        if (sync_mode) { /* wait message */
            
            char *topic = NULL;
            int payloadlen = 0;
            void *payload = NULL;

            msg_bus_recv(client, &topic, &payload, &payloadlen, 3000);

            if (topic && payload) {
                LOG_I("Recv: [%s] %s", topic, (char *)payload);
                msg_bus_free(topic, payload);
            }
        }
#endif
        sleep(1);
    }
    
    LOG_I("[Module] Template end.");
    msg_bus_deinit(client);
    log_deinit();

    return 0;
}