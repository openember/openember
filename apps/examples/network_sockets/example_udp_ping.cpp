#include "openember/osal/socket.h"

#include <cstring>
#include <string>
#include <thread>

#define LOG_TAG "example_udp_ping"
#include "openember.h"

static void udp_server_thread(uint16_t port)
{
    LOG_I("[server] start, bind 127.0.0.1:%u", (unsigned)port);
    oe_socket_t s {};
    if (oe_socket_open_udp(&s, "127.0.0.1", port) != OE_OK) {
        LOG_E("udp server: open failed");
        return;
    }

    LOG_I("[server] waiting for datagram...");
    char buf[128];
    size_t got = 0;
    oe_sockaddr_t from {};
    if (oe_socket_recvfrom(&s, buf, sizeof(buf), &got, &from, 5000) != OE_OK) {
        LOG_E("udp server: recvfrom failed");
        (void)oe_socket_close(&s);
        return;
    }
    LOG_I("[server] received %zu bytes, reply", got);

    size_t sent = 0;
    if (oe_socket_sendto(&s, buf, got, &from, &sent, 5000) != OE_OK) {
        LOG_E("udp server: sendto failed");
    }
    LOG_I("[server] sent %zu bytes, done", sent);
    (void)oe_socket_close(&s);
}

int main(int argc, char **argv)
{
    oe_log_init(LOG_TAG);
    uint16_t port = 5567;
    if (argc > 1) {
        port = static_cast<uint16_t>(std::stoi(argv[1]));
    }

    LOG_I("[main] starting server thread...");
    std::thread th(udp_server_thread, port);
    std::this_thread::sleep_for(std::chrono::milliseconds(80));

    oe_socket_t c {};
    if (oe_socket_open_udp(&c, "127.0.0.1", 0) != OE_OK) {
        LOG_E("udp client: open failed");
        th.join();
        return 1;
    }
    if (oe_socket_udp_connect(&c, "127.0.0.1", port) != OE_OK) {
        LOG_E("udp client: connect failed");
        (void)oe_socket_close(&c);
        th.join();
        return 1;
    }
    LOG_I("[client] connected to 127.0.0.1:%u", (unsigned)port);

    const char *msg = "ping";
    size_t sent = 0;
    LOG_I("[client] send: \"%s\"", msg);
    if (oe_socket_send(&c, msg, std::strlen(msg) + 1, &sent, 5000) != OE_OK) {
        LOG_E("udp client: send failed");
        (void)oe_socket_close(&c);
        th.join();
        return 1;
    }

    char buf[128];
    size_t got = 0;
    if (oe_socket_recv(&c, buf, sizeof(buf), &got, 5000) != OE_OK) {
        LOG_E("udp client: recv failed");
        (void)oe_socket_close(&c);
        th.join();
        return 1;
    }

    LOG_I("[client] pong: \"%s\" (%zu bytes)", buf, got);
    (void)oe_socket_close(&c);
    th.join();
    LOG_I("[main] exit");
    return 0;
}

