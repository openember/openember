#include "socket_uri.h"

#include <cstring>
#include <iostream>
#include <thread>

using openember::network::Socket;

static oe_result_t recv_exact(Socket &s, void *buf, size_t len, int timeout_ms)
{
    size_t got = 0;
    while (got < len) {
        size_t rd = 0;
        oe_result_t r = s.recv(static_cast<uint8_t *>(buf) + got, len - got, &rd, timeout_ms);
        if (r != OE_OK) {
            return r;
        }
        got += rd;
    }
    return OE_OK;
}

static oe_result_t send_exact(Socket &s, const void *buf, size_t len, int timeout_ms)
{
    size_t sent = 0;
    while (sent < len) {
        size_t wr = 0;
        oe_result_t r = s.send(static_cast<const uint8_t *>(buf) + sent, len - sent, &wr, timeout_ms);
        if (r != OE_OK) {
            return r;
        }
        sent += wr;
    }
    return OE_OK;
}

static void server_thread(std::string uri)
{
    std::cout << "[server] start, uri=" << uri << "\n";
    Socket server;
    if (server.listen(uri, 8) != OE_OK) {
        std::cerr << "server.listen failed\n";
        return;
    }

    std::cout << "[server] listening, waiting for client...\n";
    Socket client;
    if (server.accept(client, 5000) != OE_OK) {
        std::cerr << "server.accept timeout/fail\n";
        return;
    }
    std::cout << "[server] client accepted\n";

    uint32_t n = 0;
    if (recv_exact(client, &n, sizeof(n), 5000) != OE_OK) {
        std::cerr << "server.recv(len) failed\n";
        return;
    }
    if (n > 1024) {
        std::cerr << "server: message too large\n";
        return;
    }
    std::string msg;
    msg.resize(n);
    if (recv_exact(client, msg.data(), msg.size(), 5000) != OE_OK) {
        std::cerr << "server.recv failed\n";
        return;
    }
    std::cout << "[server] received " << msg.size() << " bytes, echo back\n";
    (void)send_exact(client, &n, sizeof(n), 5000);
    (void)send_exact(client, msg.data(), msg.size(), 5000);
    std::cout << "[server] done\n";
}

int main(int argc, char **argv)
{
    std::string uri = "unix:///tmp/openember_unix_echo.sock";
    if (argc > 1) {
        uri = argv[1];
    }

    std::cout << "[main] starting server thread...\n";
    std::thread th(server_thread, uri);
    std::this_thread::sleep_for(std::chrono::milliseconds(80));

    std::cout << "[client] connect to " << uri << "\n";
    Socket c;
    if (c.connect(uri) != OE_OK) {
        std::cerr << "client.connect failed\n";
        th.join();
        return 1;
    }
    std::cout << "[client] connected\n";

    const std::string msg = "hello-unix";
    const uint32_t n = static_cast<uint32_t>(msg.size());
    std::cout << "[client] send: \"" << msg << "\"\n";
    if (send_exact(c, &n, sizeof(n), 5000) != OE_OK || send_exact(c, msg.data(), msg.size(), 5000) != OE_OK) {
        std::cerr << "client.send failed\n";
        th.join();
        return 1;
    }

    uint32_t rn = 0;
    if (recv_exact(c, &rn, sizeof(rn), 5000) != OE_OK) {
        std::cerr << "client.recv(len) failed\n";
        th.join();
        return 1;
    }
    std::string rmsg;
    rmsg.resize(rn);
    if (recv_exact(c, rmsg.data(), rmsg.size(), 5000) != OE_OK) {
        std::cerr << "client.recv failed\n";
        th.join();
        return 1;
    }

    std::cout << "[client] echo: \"" << rmsg << "\"\n";
    th.join();
    std::cout << "[main] exit\n";
    return 0;
}

