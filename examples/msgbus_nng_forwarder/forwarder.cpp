/*
 * NNG msgbus forwarder
 *
 * Bridge model (NNG compatible):
 * - Module publishers LISTEN on IN_URL (PUB socket listen)
 * - Forwarder subscribes by DIALing IN_URL using SUB
 * - Forwarder publishes by LISTENing on OUT_URL using PUB
 * - Modules subscribers DIAL OUT_URL using SUB
 *
 * Forwarder forwards raw frames unchanged:
 *   [topic bytes][0x00][payload bytes]
 */

#include <nng/nng.h>

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define LOG_TAG "msgbus_nng_forwarder"
#include "openember/log.h"

static volatile int g_running = 1;

static void _on_signal(int sig)
{
    (void)sig;
    g_running = 0;
}

int main(int argc, char *argv[])
{
    const char *in_url = (argc > 1) ? argv[1] : "ipc:///tmp/openember-msgbus-in.ipc";
    const char *out_url = (argc > 2) ? argv[2] : "ipc:///tmp/openember-msgbus-out.ipc";

    signal(SIGINT, _on_signal);
    signal(SIGTERM, _on_signal);

    // NNG requires initialization before first use.
    nng_init_params params = {0};
    int rc = nng_init(&params);
    if (rc != 0) {
        LOG_E("nng_init failed: %d", rc);
        return 1;
    }

    nng_socket sub_sock;
    nng_socket pub_sock;

    int rv = nng_sub0_open(&sub_sock);
    if (rv != 0) {
        LOG_E("nng_sub0_open failed: %d", rv);
        return 1;
    }

    rv = nng_sub0_socket_subscribe(sub_sock, "", 0u);
    if (rv != 0) {
        LOG_E("nng_sub0_socket_subscribe failed: %d", rv);
        nng_socket_close(sub_sock);
        return 1;
    }

    // Use NONBLOCK and retry until modules create their listeners.
    while (g_running) {
        rv = nng_dial(sub_sock, in_url, NULL, NNG_FLAG_NONBLOCK);
        if (rv == 0) {
            break;
        }
        LOG_W("waiting for publisher listener: rc=%d url=%s", rv, in_url);
        sleep(1);
    }
    if (!g_running) {
        nng_socket_close(sub_sock);
        return 1;
    }

    rv = nng_pub0_open(&pub_sock);
    if (rv != 0) {
        LOG_E("nng_pub0_open failed: %d", rv);
        nng_socket_close(sub_sock);
        return 1;
    }

    // Ensure recv loop can exit promptly after g_running becomes false.
    (void)nng_socket_set_ms(sub_sock, NNG_OPT_RECVTIMEO, 1000);

    if (strncmp(out_url, "ipc://", 6) == 0) {
        // Remove stale ipc socket file (avoid EADDRINUSE on restart).
        (void)unlink(out_url + 6);
    }
    // On TCP, we cannot unlink the address. If a previous forwarder is still
    // running (port in use), retry for a short window so rebuild+restart
    // becomes deterministic.
    const int max_listen_tries = 30;
    int listen_try = 0;
    while (g_running) {
        rv = nng_listen(pub_sock, out_url, NULL, 0);
        if (rv == 0) {
            break;
        }

        if (rv == NNG_EADDRINUSE && listen_try < max_listen_tries) {
            LOG_W("out_url in use, retry listen (%d/%d) rc=%d url=%s",
                  listen_try + 1, max_listen_tries, rv, out_url);
            nng_socket_close(pub_sock);
            rv = nng_pub0_open(&pub_sock);
            if (rv != 0) {
                LOG_E("nng_pub0_open failed after retry: %d", rv);
                nng_socket_close(sub_sock);
                return 1;
            }
            listen_try++;
            sleep(1);
            continue;
        }

        LOG_E("nng_listen(pub) failed: %d url=%s", rv, out_url);
        nng_socket_close(sub_sock);
        nng_socket_close(pub_sock);
        return 1;
    }
    if (!g_running) {
        nng_socket_close(sub_sock);
        nng_socket_close(pub_sock);
        return 1;
    }

    oe_log_init(LOG_TAG);
    LOG_I("running");
    LOG_I("IN  (modules PUB listen -> forwarder SUB dial): %s", in_url);
    LOG_I("OUT (forwarder PUB -> subscribers): %s", out_url);

    while (g_running) {
        nng_msg *msg = NULL;
        rv = nng_recvmsg(sub_sock, &msg, 0);
        if (rv != 0) {
            if (!g_running) {
                break;
            }
            // Ignore temporary errors, keep forwarding.
            continue;
        }

        rv = nng_sendmsg(pub_sock, msg, 0);
        if (rv != 0) {
            // On send failure, we still own msg and must free it.
            nng_msg_free(msg);
        }
    }

    nng_socket_close(sub_sock);
    nng_socket_close(pub_sock);
    nng_fini();
    return 0;
}

