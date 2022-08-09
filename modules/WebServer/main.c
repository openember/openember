/*
 * Copyright (c) 2006-2020, DreamGrow Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2022-07-21     luhuadong    the first version
 */

#include "mongoose.h"

#define MODULE_NAME            "WebServer"
#define LOG_TAG                MODULE_NAME
#include "agloo.h"

static msg_node_t client;

#define WEB_ROOT    "/opt/agloo/web_root"
//#define WEB_ROOT    "../modules/WebServer/web_src/dist"
//#define WEB_ROOT    "web_root"
#define WEB_PORT    "0.0.0.0:8000"

static const char *s_debug_level = "2";    // debug level, from 0 to 4
static const char *s_root_dir = WEB_ROOT;
static const char *s_listening_address = WEB_PORT;
static const char *s_enable_hexdump = "no";
static const char *s_ssi_pattern = "#.html";

// Handle interrupts, like Ctrl-C
static int s_signo;
static void signal_handler(int signo)
{
    s_signo = signo;
}

static void fn(struct mg_connection *c, int ev, void *ev_data, void *fn_data)
{
    struct mg_http_serve_opts opts = {.root_dir = s_root_dir};   // Serve local dir

    if (ev == MG_EV_HTTP_MSG) {
        mg_http_serve_dir(c, ev_data, &opts);
    }
}

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
    rc = msg_bus_subscribe(client, MOD_REGISTER_REPLY_TOPIC);
    if (rc != AG_EOK) cn++;

    if (cn != 0) {
        msg_bus_deinit(client);
        printf("Message bus subscribe failed.\n");
        return -AG_ERROR;
    }

    return AG_EOK;
}

int main()
{
    int rc;
    struct mg_mgr mgr;

    sayHello(MODULE_NAME);
    
    log_init(MODULE_NAME);
    LOG_I("Version: %lu.%lu.%lu", AG_VERSION, AG_SUBVERSION, AG_REVISION);

    rc = msg_init();
    if (rc != AG_EOK) {
        LOG_E("Message channel init failed.");
        exit(1);
    }

    rc = msg_smm_register(client, MODULE_NAME, SUBMODULE_CLASS_WEB);
    if (rc != AG_EOK) {
        LOG_E("Module register failed.");
        exit(1);
    }

    /* Run web server */
    mg_mgr_init(&mgr);

    /* Create listening connection */
    if (NULL == mg_http_listen(&mgr, s_listening_address, fn, NULL)) {
        LOG_E("Exit.");
        mg_mgr_free(&mgr);
        exit(1);
    }

    // Start infinite event loop
    MG_INFO(("Mongoose version : v%s", MG_VERSION));
    MG_INFO(("Listening on     : %s", s_listening_address));
    MG_INFO(("Web root         : [%s]", s_root_dir));

    while (s_signo == 0) {
        mg_mgr_poll(&mgr, 1000);    // Block forever
    }

    mg_mgr_free(&mgr);
    MG_INFO(("Exiting on signal %d", s_signo));
    return 0;
}
