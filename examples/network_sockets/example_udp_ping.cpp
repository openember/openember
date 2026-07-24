#include "openember/osal/socket.hpp"

#include <chrono>
#include <cstring>
#include <string>
#include <thread>

#define LOG_TAG "example_udp_ping"
#include "openember.h"

using openember::osal::kOk;
using openember::osal::Socket;
using openember::osal::SockAddr;

static void udp_server_thread(uint16_t port)
{
    LOG_I("[server] start, bind 127.0.0.1:%u", static_cast<unsigned>(port));
    Socket s;
    if (s.open_udp("127.0.0.1", port) != kOk) {
        LOG_E("udp server: open failed");
        return;
    }

    LOG_I("[server] waiting for datagram...");
    char buf[128];
    size_t got = 0;
    SockAddr from {};
    if (s.recvfrom(buf, sizeof(buf), &got, &from, 5000) != kOk) {
        LOG_E("udp server: recvfrom failed");
        (void)s.close();
        return;
    }
    LOG_I("[server] received %zu bytes, reply", got);

    size_t sent = 0;
    if (s.sendto(buf, got, &from, &sent, 5000) != kOk) {
        LOG_E("udp server: sendto failed");
    }
    LOG_I("[server] sent %zu bytes, done", sent);
    (void)s.close();
}

int main(int argc, char** argv)
{
    oe_log_init(LOG_TAG);
    uint16_t port = 5567;
    if (argc > 1) {
        port = static_cast<uint16_t>(std::stoi(argv[1]));
    }

    LOG_I("[main] starting server thread...");
    std::thread th(udp_server_thread, port);
    std::this_thread::sleep_for(std::chrono::milliseconds(80));

    Socket c;
    if (c.open_udp("127.0.0.1", 0) != kOk) {
        LOG_E("udp client: open failed");
        th.join();
        return 1;
    }
    if (c.udp_connect("127.0.0.1", port) != kOk) {
        LOG_E("udp client: connect failed");
        (void)c.close();
        th.join();
        return 1;
    }
    LOG_I("[client] connected to 127.0.0.1:%u", static_cast<unsigned>(port));

    const char* msg = "ping";
    size_t sent = 0;
    LOG_I("[client] send: \"%s\"", msg);
    if (c.send(msg, std::strlen(msg) + 1, &sent, 5000) != kOk) {
        LOG_E("udp client: send failed");
        (void)c.close();
        th.join();
        return 1;
    }

    char buf[128];
    size_t got = 0;
    if (c.recv(buf, sizeof(buf), &got, 5000) != kOk) {
        LOG_E("udp client: recv failed");
        (void)c.close();
        th.join();
        return 1;
    }

    LOG_I("[client] pong: \"%s\" (%zu bytes)", buf, got);
    (void)c.close();
    th.join();
    LOG_I("[main] exit");
    return 0;
}
