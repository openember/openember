#include "minunit.h"

#include "openember/osal/cond.hpp"
#include "openember/osal/event.hpp"
#include "openember/osal/fifo.hpp"
#include "openember/osal/mq.hpp"
#include "openember/osal/mutex.hpp"
#include "openember/osal/pipe.hpp"
#include "openember/osal/sem.hpp"
#include "openember/osal/shm.hpp"
#include "openember/osal/signals.hpp"
#include "openember/osal/socket.hpp"
#include "openember/osal/thread.hpp"
#include "openember/osal/time.hpp"
#include "openember/osal/types.hpp"

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <signal.h>
#include <unistd.h>

using openember::osal::Cond;
using openember::osal::Event;
using openember::osal::Fifo;
using openember::osal::FifoMode;
using openember::osal::MessageQueue;
using openember::osal::MessageQueueCaps;
using openember::osal::Mutex;
using openember::osal::Pipe;
using openember::osal::PipeCaps;
using openember::osal::Result;
using openember::osal::Semaphore;
using openember::osal::SharedMemory;
using openember::osal::ShmCaps;
using openember::osal::SignalInfo;
using openember::osal::SignalWaiter;
using openember::osal::SignalsCaps;
using openember::osal::Socket;
using openember::osal::SocketCaps;
using openember::osal::Thread;
using openember::osal::clock_monotonic_ns;
using openember::osal::kErrAgain;
using openember::osal::kErrBusy;
using openember::osal::kErrInternal;
using openember::osal::kErrInvalidArg;
using openember::osal::kErrTimeout;
using openember::osal::kErrUnsupported;
using openember::osal::kOk;
using openember::osal::sleep_ms;


static volatile int g_counter;

static void inc_worker(void *arg)
{
    int *n = (int *)arg;
    int i;

    for (i = 0; i < *n; i++) {
        g_counter++;
    }
}

MU_TEST(test_mutex_lock_unlock)
{
    Mutex m;
    Result r;

    g_counter = 0;
    r = m.lock();
    mu_assert(r == kOk, "lock");
    g_counter = 42;
    r = m.unlock();
    mu_assert(r == kOk, "unlock");

    mu_assert_int_eq(42, g_counter);
}

MU_TEST(test_thread_join)
{
    Thread th;
    int n = 1000;
    Result r;

    g_counter = 0;
    r = th.start([&n]{ inc_worker(&n); }); mu_assert(r == kOk, "thread create");

    r = th.join();
    mu_assert(r == kOk, "thread join");
    mu_assert_int_eq(1000, g_counter);
}

MU_TEST(test_time_monotonic)
{
    uint64_t a = 0;
    uint64_t b = 0;
    Result r;

    r = clock_monotonic_ns(&a);
    mu_assert(r == kOk, "clock a");
    r = sleep_ms(10);
    mu_assert(r == kOk, "sleep");
    r = clock_monotonic_ns(&b);
    mu_assert(r == kOk, "clock b");
    mu_check(b > a);
}

MU_TEST(test_cond_timeout)
{
    Mutex m;
    Cond c;
    Result r;

    r = m.lock();
    mu_assert(r == kOk, "mutex lock");
    r = c.wait_timeout_ms(m, 10);
    mu_assert(r == kErrTimeout, "cond timedwait timeout");
    r = m.unlock();
    mu_assert(r == kOk, "mutex unlock");

    }

MU_TEST(test_event_timeout_and_set)
{
    Event e;
    Result r;

    r = e.wait_timeout_ms( 5);
    mu_assert(r == kErrTimeout, "event wait timeout");

    r = e.set();
    mu_assert(r == kOk, "event set");

    r = e.wait_timeout_ms( 0);
    mu_assert(r == kOk, "event wait after set");

    }

MU_TEST(test_sem_trywait_timeout_and_post)
{
    Semaphore s(0);
    Result r;
    r = s.try_wait();
    mu_assert(r == kErrAgain, "sem trywait empty");

    r = s.wait_timeout_ms( 5);
    mu_assert(r == kErrTimeout, "sem timedwait timeout");

    r = s.post();
    mu_assert(r == kOk, "sem post");

    r = s.wait_timeout_ms( 0);
    mu_assert(r == kOk, "sem wait after post");

    }

MU_TEST(test_shm_create_open_ptr_unlink)
{
    SharedMemory shm1;
    SharedMemory shm2;
    Result r;
    char name[128];
    void *addr = NULL;
    size_t size = 0;

    /* Unique name for local test */
    snprintf(name, sizeof(name), "/oe_shm_test_%d_%u", (int)getpid(), (unsigned)rand());

    r = SharedMemory::unlink(name);
    (void)r;

    r = shm1.create(name, 4096);
    mu_assert(r == kOk, "shm create");

    r = shm1.get_ptr( &addr, &size);
    mu_assert(r == kOk, "shm get ptr");
    mu_assert(size >= 4096, "shm size");

    /* Write value into shared memory */
    ((volatile uint32_t *)addr)[0] = 0xA5A5A5A5u;

    r = shm1.close();
    mu_assert(r == kOk, "shm close 1");

    r = shm2.open(name);
    mu_assert(r == kOk, "shm open");

    addr = NULL;
    size = 0;
    r = shm2.get_ptr( &addr, &size);
    mu_assert(r == kOk, "shm get ptr 2");
    mu_assert(((volatile uint32_t *)addr)[0] == 0xA5A5A5A5u, "shm value preserved");

    r = shm2.close();
    mu_assert(r == kOk, "shm close 2");

    r = SharedMemory::unlink(name);
    mu_assert(r == kOk, "shm unlink");
}

typedef struct {
    Socket server;
    uint32_t expected;
    uint32_t response;

    Result accept_res;
    Result recv_res;
    Result send_res;
    uint32_t recv_value;
} socket_test_ctx_t;

static void socket_server_worker(void *arg)
{
    socket_test_ctx_t *ctx = (socket_test_ctx_t *)arg;
    Socket client;
    size_t got = 0;
    uint32_t v = 0;
    uint32_t resp = ctx->response;

    ctx->accept_res = ctx->server.accept(client, -1);
    if (ctx->accept_res != kOk) {
        return;
    }

    ctx->recv_res = client.recv( &v, sizeof(v), &got, -1);
    ctx->recv_value = v;
    ctx->send_res = client.send( &resp, sizeof(resp), &got, -1);

    (void)client.close();
    (void)got;
}

MU_TEST(test_socket_accept_timeout)
{
    Socket server;
    Socket client;
    Result r;
    char path[108];

    snprintf(path, sizeof(path), "/tmp/oe_sock_test_%d_%u.sock", (int)getpid(), (unsigned)rand());

    r = server.open_unix_server(path, 8);
    mu_assert(r == kOk, "socket open server");

    r = server.accept(client, 10);
    mu_assert(r == kErrTimeout, "socket accept timeout");

    r = server.close();
    mu_assert(r == kOk, "socket close server");
}

MU_TEST(test_socket_unix_send_recv)
{
    Result r;
    socket_test_ctx_t ctx;
    Thread th;

    char path[108];
    snprintf(path, sizeof(path), "/tmp/oe_sock_test_%d_%u.sock", (int)getpid(), (unsigned)rand());

    ctx.expected = 0x11223344u;
    ctx.response = 0x55667788u;

    r = ctx.server.open_unix_server( path, 8);
    mu_assert(r == kOk, "socket open server");

    r = th.start([&ctx]{ socket_server_worker(&ctx); }); mu_assert(r == kOk, "thread create");

    Socket cli;
    r = cli.open_unix_client(path);
    mu_assert(r == kOk, "socket open client");

    size_t sent = 0;
    r = cli.send( &ctx.expected, sizeof(ctx.expected), &sent, 1000);
    mu_assert(r == kOk, "socket send");

    uint32_t got_resp = 0;
    size_t recvd = 0;
    r = cli.recv( &got_resp, sizeof(got_resp), &recvd, 1000);
    mu_assert(r == kOk, "socket recv");
    mu_assert(got_resp == ctx.response, "socket response value");

    r = cli.close();
    mu_assert(r == kOk, "socket close client");

    r = th.join();
    mu_assert(r == kOk, "thread join");
    mu_assert(ctx.accept_res == kOk, "server accept ok");
    mu_assert(ctx.recv_res == kOk, "server recv ok");
    mu_assert(ctx.send_res == kOk, "server send ok");
    mu_assert(ctx.recv_value == ctx.expected, "server recv value match");

    r = ctx.server.close();
    mu_assert(r == kOk, "socket close server");
}

typedef struct {
    Pipe pipe;
    uint32_t expected;
    Result read_res;
    uint32_t read_value;
} pipe_test_ctx_t;

static void pipe_reader_worker(void *arg)
{
    pipe_test_ctx_t *ctx = (pipe_test_ctx_t *)arg;
    Result r;
    size_t got = 0;
    uint32_t v = 0;

    r = ctx->pipe.read( &v, sizeof(v), &got, -1);
    ctx->read_res = r;
    ctx->read_value = v;
}

MU_TEST(test_pipe_read_write)
{
    pipe_test_ctx_t ctx;
    Result r;
    Thread th;

    ctx.expected = 0xCAFEBABEu;

    r = ctx.pipe.create();
    mu_assert(r == kOk, "pipe create");

    r = th.start([&ctx]{ pipe_reader_worker(&ctx); }); mu_assert(r == kOk, "pipe reader thread create");

    size_t sent = 0;
    r = ctx.pipe.write( &ctx.expected, sizeof(ctx.expected), &sent, 1000);
    mu_assert(r == kOk, "pipe write");

    r = th.join();
    mu_assert(r == kOk, "pipe reader thread join");
    mu_assert(ctx.read_res == kOk, "pipe read ok");
    mu_assert(ctx.read_value == ctx.expected, "pipe read value match");

    r = ctx.pipe.close();
    mu_assert(r == kOk, "pipe close");
}

typedef struct {
    Fifo fifo;
    uint32_t expected;
    Result read_res;
    uint32_t read_value;
} fifo_test_ctx_t;

static void fifo_reader_worker(void *arg)
{
    fifo_test_ctx_t *ctx = (fifo_test_ctx_t *)arg;
    Result r;
    size_t got = 0;
    uint32_t v = 0;

    r = ctx->fifo.read( &v, sizeof(v), &got, -1);
    ctx->read_res = r;
    ctx->read_value = v;
}

MU_TEST(test_fifo_read_write)
{
    fifo_test_ctx_t ctx;
    Result r;
    Thread th;
    char path[128];

    snprintf(path, sizeof(path), "/tmp/oe_fifo_test_%d_%u.fifo", (int)getpid(), (unsigned)rand());
    (void)Fifo::unlink(path);

    ctx.expected = 0x12345678u;

    /* Open fifo for reading inside worker thread context */
    r = ctx.fifo.open(path, FifoMode::Read);
    mu_assert(r == kOk, "fifo open read");

    r = th.start([&ctx]{ fifo_reader_worker(&ctx); }); mu_assert(r == kOk, "fifo reader thread create");

    Fifo writer;
    r = writer.open(path, FifoMode::Write);
    mu_assert(r == kOk, "fifo open write");

    size_t sent = 0;
    r = writer.write( &ctx.expected, sizeof(ctx.expected), &sent, 1000);
    mu_assert(r == kOk, "fifo write");

    r = th.join();
    mu_assert(r == kOk, "fifo reader thread join");

    mu_assert(ctx.read_res == kOk, "fifo read ok");
    mu_assert(ctx.read_value == ctx.expected, "fifo read value match");

    r = writer.close();
    mu_assert(r == kOk, "fifo writer close");

    r = ctx.fifo.close();
    mu_assert(r == kOk, "fifo reader close");

    r = Fifo::unlink(path);
    mu_assert(r == kOk, "fifo unlink");
}

typedef struct {
    int signum;
} signals_sender_ctx_t;

static void signals_sender_worker(void *arg)
{
    signals_sender_ctx_t *ctx = (signals_sender_ctx_t *)arg;
    (void)kill(getpid(), ctx->signum);
}

MU_TEST(test_signals_wait_timeout_and_ok)
{
    SignalWaiter sigs;
    SignalsCaps caps;
    SignalInfo info;
    Result r;

    int signum = SIGUSR1;
    r = SignalWaiter::query_caps(&caps);
    mu_assert(r == kOk, "signals query caps");
    mu_check(caps.supports_signalfd == 1u);

    r = sigs.open(std::vector<int>{signum});
    mu_assert(r == kOk, "signals open");

    r = sigs.wait(&info, 10);
    mu_assert(r == kErrTimeout, "signals wait timeout");

    /* Send one SIGUSR1 from another thread. */
    signals_sender_ctx_t sctx;
    Thread th;
    sctx.signum = signum;

    r = th.start([&sctx]{ signals_sender_worker(&sctx); }); mu_assert(r == kOk, "signals sender thread create");

    r = sigs.wait(&info, 1000);
    mu_assert(r == kOk, "signals wait ok");
    mu_assert(info.signum == signum, "signals wait signum match");

    r = th.join();
    mu_assert(r == kOk, "signals sender thread join");

    r = sigs.close();
    mu_assert(r == kOk, "signals close");
}

MU_TEST(test_mq_send_recv_optional)
{
    MessageQueue mq;
    Result r;
    MessageQueueCaps caps;
    char name[128];
    char send_buf[32];
    char recv_buf[32];
    size_t recv_len = 0;
    const char *msg = "oe_mq_test";
    size_t msg_len = strlen(msg);

    snprintf(name, sizeof(name), "/oe_mq_test_%d_%u", (int)getpid(), (unsigned)rand());
    (void)MessageQueue::unlink(name);

    r = MessageQueue::query_caps(&caps);
    mu_assert(r == kOk, "mq query caps");
    mu_check(caps.supports_message_queue == 1u);

    r = mq.create( name, 10, sizeof(send_buf));
    if (r == kErrUnsupported) {
        /* Environment may not support POSIX mqueue (e.g. /dev/mqueue not available) */
        return;
    }
    mu_assert(r == kOk, "mq create");

    memset(send_buf, 0, sizeof(send_buf));
    memcpy(send_buf, msg, msg_len);

    r = mq.send( send_buf, msg_len, 1000);
    mu_assert(r == kOk, "mq send");

    memset(recv_buf, 0, sizeof(recv_buf));
    r = mq.recv( recv_buf, sizeof(recv_buf), &recv_len, 1000);
    mu_assert(r == kOk, "mq recv");
    mu_assert(recv_len == msg_len, "mq recv length match");
    mu_assert(memcmp(recv_buf, msg, msg_len) == 0, "mq message match");

    r = mq.close();
    mu_assert(r == kOk, "mq close");
    r = MessageQueue::unlink(name);
    mu_assert(r == kOk, "mq unlink");
}

MU_TEST(test_socket_tcp_send_recv)
{
    Result r;
    Socket server;
    Socket client;
    Socket accepted;
    uint16_t port = 0;

    r = server.open_tcp_server( "127.0.0.1", 0, 8);
    mu_assert(r == kOk, "tcp server open");

    r = server.get_local_port( &port);
    mu_assert(r == kOk, "tcp server local port");
    mu_check(port != 0);

    r = client.open_tcp_client( "localhost", port, 1000);
    mu_assert(r == kOk, "tcp client open");

    r = server.accept(accepted, 1000);
    mu_assert(r == kOk, "tcp accept");

    uint32_t req = 0x11111111u;
    uint32_t resp = 0x22222222u;
    size_t sent = 0;
    size_t recvd = 0;

    r = client.send( &req, sizeof(req), &sent, 1000);
    mu_assert(r == kOk, "tcp client send");

    uint32_t got_req = 0;
    r = accepted.recv( &got_req, sizeof(got_req), &recvd, 1000);
    mu_assert(r == kOk, "tcp server recv");
    mu_assert(got_req == req, "tcp server req match");

    r = accepted.send( &resp, sizeof(resp), &sent, 1000);
    mu_assert(r == kOk, "tcp server send");

    uint32_t got_resp = 0;
    r = client.recv( &got_resp, sizeof(got_resp), &recvd, 1000);
    mu_assert(r == kOk, "tcp client recv");
    mu_assert(got_resp == resp, "tcp client resp match");

    (void)accepted.close();
    (void)client.close();
    (void)server.close();
}

MU_TEST(test_socket_udp_send_recv)
{
    Result r;
    Socket rx;
    Socket tx;
    uint16_t port = 0;

    r = rx.open_udp( "127.0.0.1", 0);
    mu_assert(r == kOk, "udp rx open");

    r = rx.get_local_port( &port);
    mu_assert(r == kOk, "udp rx local port");
    mu_check(port != 0);

    r = tx.open_udp( "127.0.0.1", 0);
    mu_assert(r == kOk, "udp tx open");

    r = tx.udp_connect( "127.0.0.1", port);
    mu_assert(r == kOk, "udp connect");

    const char msg[] = "hello-udp";
    size_t sent = 0;
    r = tx.send( msg, sizeof(msg), &sent, 1000);
    mu_assert(r == kOk, "udp send");

    char buf[32];
    size_t got = 0;
    memset(buf, 0, sizeof(buf));
    r = rx.recv( buf, sizeof(buf), &got, 1000);
    mu_assert(r == kOk, "udp recv");
    mu_check(got > 0);
    mu_assert(memcmp(buf, msg, sizeof(msg)) == 0, "udp payload match");

    (void)tx.close();
    (void)rx.close();
}

MU_TEST_SUITE(osal_suite)
{
    MU_SUITE_CONFIGURE(NULL, NULL);
    MU_RUN_TEST(test_mutex_lock_unlock);
    MU_RUN_TEST(test_thread_join);
    MU_RUN_TEST(test_time_monotonic);
    MU_RUN_TEST(test_cond_timeout);
    MU_RUN_TEST(test_event_timeout_and_set);
    MU_RUN_TEST(test_sem_trywait_timeout_and_post);
    MU_RUN_TEST(test_shm_create_open_ptr_unlink);
    MU_RUN_TEST(test_socket_accept_timeout);
    MU_RUN_TEST(test_socket_unix_send_recv);
    MU_RUN_TEST(test_socket_tcp_send_recv);
    MU_RUN_TEST(test_socket_udp_send_recv);
    MU_RUN_TEST(test_pipe_read_write);
    MU_RUN_TEST(test_fifo_read_write);
    MU_RUN_TEST(test_signals_wait_timeout_and_ok);
    MU_RUN_TEST(test_mq_send_recv_optional);
}

int main(void)
{
    MU_RUN_SUITE(osal_suite);
    MU_REPORT();
    return minunit_fail > 0;
}
