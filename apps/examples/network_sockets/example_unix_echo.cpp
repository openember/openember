#include "socket_uri.h"

#include <cstring>
#include <thread>

#define LOG_TAG "example_unix_echo"
#include "openember.h"

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
    LOG_I("[server] start, uri=%s", uri.c_str());
    Socket server;
    if (server.listen(uri, 8) != OE_OK) {
        LOG_E("server.listen failed");
        return;
    }

    LOG_I("[server] listening, waiting for client...");
    Socket client;
    if (server.accept(client, 5000) != OE_OK) {
        LOG_E("server.accept timeout/fail");
        return;
    }
    LOG_I("[server] client accepted");

    uint32_t n = 0;
    if (recv_exact(client, &n, sizeof(n), 5000) != OE_OK) {
        LOG_E("server.recv(len) failed");
        return;
    }
    if (n > 1024) {
        LOG_E("server: message too large");
        return;
    }
    std::string msg;
    msg.resize(n);
    if (recv_exact(client, msg.data(), msg.size(), 5000) != OE_OK) {
        LOG_E("server.recv failed");
        return;
    }
    LOG_I("[server] received %zu bytes, echo back", msg.size());
    (void)send_exact(client, &n, sizeof(n), 5000);
    (void)send_exact(client, msg.data(), msg.size(), 5000);
    LOG_I("[server] done");
}

int main(int argc, char **argv)
{
    oe_log_init(LOG_TAG);
    std::string uri = "unix:///tmp/openember_unix_echo.sock";
    if (argc > 1) {
        uri = argv[1];
    }

    LOG_I("[main] starting server thread...");
    std::thread th(server_thread, uri);
    std::this_thread::sleep_for(std::chrono::milliseconds(80));

    LOG_I("[client] connect to %s", uri.c_str());
    Socket c;
    if (c.connect(uri) != OE_OK) {
        LOG_E("client.connect failed");
        th.join();
        return 1;
    }
    LOG_I("[client] connected");

    const std::string msg = "hello-unix";
    const uint32_t n = static_cast<uint32_t>(msg.size());
    LOG_I("[client] send: \"%s\"", msg.c_str());
    if (send_exact(c, &n, sizeof(n), 5000) != OE_OK || send_exact(c, msg.data(), msg.size(), 5000) != OE_OK) {
        LOG_E("client.send failed");
        th.join();
        return 1;
    }

    uint32_t rn = 0;
    if (recv_exact(c, &rn, sizeof(rn), 5000) != OE_OK) {
        LOG_E("client.recv(len) failed");
        th.join();
        return 1;
    }
    std::string rmsg;
    rmsg.resize(rn);
    if (recv_exact(c, rmsg.data(), rmsg.size(), 5000) != OE_OK) {
        LOG_E("client.recv failed");
        th.join();
        return 1;
    }

    LOG_I("[client] echo: \"%s\"", rmsg.c_str());
    th.join();
    LOG_I("[main] exit");
    return 0;
}

