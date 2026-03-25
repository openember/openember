#include "openember/osal/socket.h"

#include <cstring>
#include <iostream>
#include <thread>

static void udp_server_thread(uint16_t port)
{
    std::cout << "[server] start, bind 127.0.0.1:" << port << "\n";
    oe_socket_t s {};
    if (oe_socket_open_udp(&s, "127.0.0.1", port) != OE_OK) {
        std::cerr << "udp server: open failed\n";
        return;
    }

    std::cout << "[server] waiting for datagram...\n";
    char buf[128];
    size_t got = 0;
    oe_sockaddr_t from {};
    if (oe_socket_recvfrom(&s, buf, sizeof(buf), &got, &from, 5000) != OE_OK) {
        std::cerr << "udp server: recvfrom failed\n";
        (void)oe_socket_close(&s);
        return;
    }
    std::cout << "[server] received " << got << " bytes, reply\n";

    size_t sent = 0;
    if (oe_socket_sendto(&s, buf, got, &from, &sent, 5000) != OE_OK) {
        std::cerr << "udp server: sendto failed\n";
    }
    std::cout << "[server] sent " << sent << " bytes, done\n";
    (void)oe_socket_close(&s);
}

int main(int argc, char **argv)
{
    uint16_t port = 5567;
    if (argc > 1) {
        port = static_cast<uint16_t>(std::stoi(argv[1]));
    }

    std::cout << "[main] starting server thread...\n";
    std::thread th(udp_server_thread, port);
    std::this_thread::sleep_for(std::chrono::milliseconds(80));

    oe_socket_t c {};
    if (oe_socket_open_udp(&c, "127.0.0.1", 0) != OE_OK) {
        std::cerr << "udp client: open failed\n";
        th.join();
        return 1;
    }
    if (oe_socket_udp_connect(&c, "127.0.0.1", port) != OE_OK) {
        std::cerr << "udp client: connect failed\n";
        (void)oe_socket_close(&c);
        th.join();
        return 1;
    }
    std::cout << "[client] connected to 127.0.0.1:" << port << "\n";

    const char *msg = "ping";
    size_t sent = 0;
    std::cout << "[client] send: \"" << msg << "\"\n";
    if (oe_socket_send(&c, msg, std::strlen(msg) + 1, &sent, 5000) != OE_OK) {
        std::cerr << "udp client: send failed\n";
        (void)oe_socket_close(&c);
        th.join();
        return 1;
    }

    char buf[128];
    size_t got = 0;
    if (oe_socket_recv(&c, buf, sizeof(buf), &got, 5000) != OE_OK) {
        std::cerr << "udp client: recv failed\n";
        (void)oe_socket_close(&c);
        th.join();
        return 1;
    }

    std::cout << "[client] pong: \"" << buf << "\" (" << got << " bytes)\n";
    (void)oe_socket_close(&c);
    th.join();
    std::cout << "[main] exit\n";
    return 0;
}

