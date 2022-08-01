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
#include "agloo.h"

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

int main()
{
    struct mg_mgr mgr;

    sayHello("Web Server");

    mg_mgr_init(&mgr);
    mg_http_listen(&mgr, s_listening_address, fn, NULL);     // Create listening connection

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
