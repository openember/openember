/*
 * Copyright (c) 2006-2020, DreamGrow Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2022-07-07     luhuadong    the first version
 * 2022-07-28     luhuadong    add message pub & sub
 */

#include <stdio.h>
#include <unistd.h>

#include "agloo.h"
#include "log_wrapper.h"
#include "msg_wrapper.h"

#include "cJSON.h"

//#define TEMPLATE_RAW_MSG

#ifdef TEMPLATE_RAW_MSG
typedef struct test_msg {
    int id;
    char *msg;
} test_msg_t;
#endif

static void temp_msg_arrived_cb(char *topic, void *payload, size_t payloadlen)
{
#ifdef TEMPLATE_RAW_MSG
    test_msg_t *test_msg = (test_msg_t *)payload;
    printf("Payload len = %lu >> id: %d, msg: %s\n", payloadlen, test_msg->id, test_msg->msg);
#else
    printf("[%s] %s\n", topic, (char *)payload);

    cJSON *json = cJSON_Parse(payload);
    printf("%s\n\n", cJSON_Print(json));
    cJSON_Delete(json);
#endif
}

int main(void)
{
    int rc;
    msg_node_t client;
    
    log_init("Template");

    LOG_I("[Module] Template");
    LOG_I("Version: %lu.%lu.%lu", AG_VERSION, AG_SUBVERSION, AG_REVISION);

    rc = msg_bus_init(&client, "Template", NULL, temp_msg_arrived_cb);
    if (rc != AG_EOK) {
        LOG_I("Message bus init failed.");
        return -1;
    }

    rc = msg_bus_subscribe(client, TEST_TOPIC);
    if (rc != AG_EOK) {
        msg_bus_deinit(client);
        LOG_I("Message bus subscribe failed.");
        return -1;
    }

    int count = 10;
    
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