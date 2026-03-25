#include "minunit.h"

#include "openember/osal/mutex.h"
#include "openember/osal/cond.h"
#include "openember/osal/event.h"
#include "openember/osal/sem.h"
#include "openember/osal/shm.h"
#include "openember/osal/socket.h"
#include "openember/osal/pipe.h"
#include "openember/osal/fifo.h"
#include "openember/osal/signals.h"
#include "openember/osal/mq.h"
#include "openember/osal/thread.h"
#include "openember/osal/time.h"
#include "openember/osal/types.h"

#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>

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
    oe_mutex_t m;
    oe_result_t r;

    g_counter = 0;
    r = oe_mutex_init(&m);
    mu_assert(r == OE_OK, "mutex init");

    r = oe_mutex_lock(&m);
    mu_assert(r == OE_OK, "lock");
    g_counter = 42;
    r = oe_mutex_unlock(&m);
    mu_assert(r == OE_OK, "unlock");

    r = oe_mutex_destroy(&m);
    mu_assert(r == OE_OK, "destroy");
    mu_assert_int_eq(42, g_counter);
}

MU_TEST(test_thread_join)
{
    oe_thread_t th;
    int n = 1000;
    oe_result_t r;

    g_counter = 0;
    r = oe_thread_create(&th, inc_worker, &n);
    mu_assert(r == OE_OK, "thread create");

    r = oe_thread_join(&th);
    mu_assert(r == OE_OK, "thread join");
    mu_assert_int_eq(1000, g_counter);
}

MU_TEST(test_time_monotonic)
{
    uint64_t a = 0;
    uint64_t b = 0;
    oe_result_t r;

    r = oe_clock_monotonic_ns(&a);
    mu_assert(r == OE_OK, "clock a");
    r = oe_sleep_ms(10);
    mu_assert(r == OE_OK, "sleep");
    r = oe_clock_monotonic_ns(&b);
    mu_assert(r == OE_OK, "clock b");
    mu_check(b > a);
}

MU_TEST(test_cond_timeout)
{
    oe_mutex_t m;
    oe_cond_t c;
    oe_result_t r;

    r = oe_mutex_init(&m);
    mu_assert(r == OE_OK, "mutex init");
    r = oe_cond_init(&c);
    mu_assert(r == OE_OK, "cond init");

    r = oe_mutex_lock(&m);
    mu_assert(r == OE_OK, "mutex lock");
    r = oe_cond_wait_timeout_ms(&c, &m, 10);
    mu_assert(r == OE_ERR_TIMEOUT, "cond timedwait timeout");
    r = oe_mutex_unlock(&m);
    mu_assert(r == OE_OK, "mutex unlock");

    r = oe_cond_destroy(&c);
    mu_assert(r == OE_OK, "cond destroy");
    r = oe_mutex_destroy(&m);
    mu_assert(r == OE_OK, "mutex destroy");
}

MU_TEST(test_event_timeout_and_set)
{
    oe_event_t e;
    oe_result_t r;

    r = oe_event_init(&e);
    mu_assert(r == OE_OK, "event init");

    r = oe_event_wait_timeout_ms(&e, 5);
    mu_assert(r == OE_ERR_TIMEOUT, "event wait timeout");

    r = oe_event_set(&e);
    mu_assert(r == OE_OK, "event set");

    r = oe_event_wait_timeout_ms(&e, 0);
    mu_assert(r == OE_OK, "event wait after set");

    r = oe_event_destroy(&e);
    mu_assert(r == OE_OK, "event destroy");
}

MU_TEST(test_sem_trywait_timeout_and_post)
{
    oe_sem_t s;
    oe_result_t r;

    r = oe_sem_init(&s, 0);
    mu_assert(r == OE_OK, "sem init");

    r = oe_sem_trywait(&s);
    mu_assert(r == OE_ERR_AGAIN, "sem trywait empty");

    r = oe_sem_wait_timeout_ms(&s, 5);
    mu_assert(r == OE_ERR_TIMEOUT, "sem timedwait timeout");

    r = oe_sem_post(&s);
    mu_assert(r == OE_OK, "sem post");

    r = oe_sem_wait_timeout_ms(&s, 0);
    mu_assert(r == OE_OK, "sem wait after post");

    r = oe_sem_destroy(&s);
    mu_assert(r == OE_OK, "sem destroy");
}

MU_TEST(test_shm_create_open_ptr_unlink)
{
    oe_shm_t shm1;
    oe_shm_t shm2;
    oe_result_t r;
    char name[128];
    void *addr = NULL;
    size_t size = 0;

    /* Unique name for local test */
    snprintf(name, sizeof(name), "/oe_shm_test_%d_%u", (int)getpid(), (unsigned)rand());

    r = oe_shm_unlink(name);
    (void)r;

    r = oe_shm_create(&shm1, name, 4096);
    mu_assert(r == OE_OK, "shm create");

    r = oe_shm_get_ptr(&shm1, &addr, &size);
    mu_assert(r == OE_OK, "shm get ptr");
    mu_assert(size >= 4096, "shm size");

    /* Write value into shared memory */
    ((volatile uint32_t *)addr)[0] = 0xA5A5A5A5u;

    r = oe_shm_close(&shm1);
    mu_assert(r == OE_OK, "shm close 1");

    r = oe_shm_open(&shm2, name);
    mu_assert(r == OE_OK, "shm open");

    addr = NULL;
    size = 0;
    r = oe_shm_get_ptr(&shm2, &addr, &size);
    mu_assert(r == OE_OK, "shm get ptr 2");
    mu_assert(((volatile uint32_t *)addr)[0] == 0xA5A5A5A5u, "shm value preserved");

    r = oe_shm_close(&shm2);
    mu_assert(r == OE_OK, "shm close 2");

    r = oe_shm_unlink(name);
    mu_assert(r == OE_OK, "shm unlink");
}

typedef struct {
    oe_socket_t server;
    uint32_t expected;
    uint32_t response;

    oe_result_t accept_res;
    oe_result_t recv_res;
    oe_result_t send_res;
    uint32_t recv_value;
} socket_test_ctx_t;

static void socket_server_worker(void *arg)
{
    socket_test_ctx_t *ctx = (socket_test_ctx_t *)arg;
    oe_socket_t client;
    size_t got = 0;
    uint32_t v = 0;
    uint32_t resp = ctx->response;

    memset(&client, 0, sizeof(client));
    ctx->accept_res = oe_socket_accept(&ctx->server, &client, -1);
    if (ctx->accept_res != OE_OK) {
        return;
    }

    ctx->recv_res = oe_socket_recv(&client, &v, sizeof(v), &got, -1);
    ctx->recv_value = v;
    ctx->send_res = oe_socket_send(&client, &resp, sizeof(resp), &got, -1);

    (void)oe_socket_close(&client);
    (void)got;
}

MU_TEST(test_socket_accept_timeout)
{
    oe_socket_t server;
    oe_socket_t client;
    oe_result_t r;
    char path[108];

    snprintf(path, sizeof(path), "/tmp/oe_sock_test_%d_%u.sock", (int)getpid(), (unsigned)rand());

    r = oe_socket_open_unix_server(&server, path, 8);
    mu_assert(r == OE_OK, "socket open server");

    memset(&client, 0, sizeof(client));
    r = oe_socket_accept(&server, &client, 10);
    mu_assert(r == OE_ERR_TIMEOUT, "socket accept timeout");

    r = oe_socket_close(&server);
    mu_assert(r == OE_OK, "socket close server");
}

MU_TEST(test_socket_unix_send_recv)
{
    oe_result_t r;
    socket_test_ctx_t ctx;
    oe_thread_t th;

    char path[108];
    snprintf(path, sizeof(path), "/tmp/oe_sock_test_%d_%u.sock", (int)getpid(), (unsigned)rand());

    memset(&ctx, 0, sizeof(ctx));
    ctx.expected = 0x11223344u;
    ctx.response = 0x55667788u;

    r = oe_socket_open_unix_server(&ctx.server, path, 8);
    mu_assert(r == OE_OK, "socket open server");

    r = oe_thread_create(&th, socket_server_worker, &ctx);
    mu_assert(r == OE_OK, "thread create");

    oe_socket_t cli;
    memset(&cli, 0, sizeof(cli));
    r = oe_socket_open_unix_client(&cli, path);
    mu_assert(r == OE_OK, "socket open client");

    size_t sent = 0;
    r = oe_socket_send(&cli, &ctx.expected, sizeof(ctx.expected), &sent, 1000);
    mu_assert(r == OE_OK, "socket send");

    uint32_t got_resp = 0;
    size_t recvd = 0;
    r = oe_socket_recv(&cli, &got_resp, sizeof(got_resp), &recvd, 1000);
    mu_assert(r == OE_OK, "socket recv");
    mu_assert(got_resp == ctx.response, "socket response value");

    r = oe_socket_close(&cli);
    mu_assert(r == OE_OK, "socket close client");

    r = oe_thread_join(&th);
    mu_assert(r == OE_OK, "thread join");
    mu_assert(ctx.accept_res == OE_OK, "server accept ok");
    mu_assert(ctx.recv_res == OE_OK, "server recv ok");
    mu_assert(ctx.send_res == OE_OK, "server send ok");
    mu_assert(ctx.recv_value == ctx.expected, "server recv value match");

    r = oe_socket_close(&ctx.server);
    mu_assert(r == OE_OK, "socket close server");
}

typedef struct {
    oe_pipe_t pipe;
    uint32_t expected;
    oe_result_t read_res;
    uint32_t read_value;
} pipe_test_ctx_t;

static void pipe_reader_worker(void *arg)
{
    pipe_test_ctx_t *ctx = (pipe_test_ctx_t *)arg;
    oe_result_t r;
    size_t got = 0;
    uint32_t v = 0;

    r = oe_pipe_read(&ctx->pipe, &v, sizeof(v), &got, -1);
    ctx->read_res = r;
    ctx->read_value = v;
}

MU_TEST(test_pipe_read_write)
{
    pipe_test_ctx_t ctx;
    oe_result_t r;
    oe_thread_t th;

    memset(&ctx, 0, sizeof(ctx));
    ctx.expected = 0xCAFEBABEu;

    r = oe_pipe_create(&ctx.pipe);
    mu_assert(r == OE_OK, "pipe create");

    r = oe_thread_create(&th, pipe_reader_worker, &ctx);
    mu_assert(r == OE_OK, "pipe reader thread create");

    size_t sent = 0;
    r = oe_pipe_write(&ctx.pipe, &ctx.expected, sizeof(ctx.expected), &sent, 1000);
    mu_assert(r == OE_OK, "pipe write");

    r = oe_thread_join(&th);
    mu_assert(r == OE_OK, "pipe reader thread join");
    mu_assert(ctx.read_res == OE_OK, "pipe read ok");
    mu_assert(ctx.read_value == ctx.expected, "pipe read value match");

    r = oe_pipe_close(&ctx.pipe);
    mu_assert(r == OE_OK, "pipe close");
}

typedef struct {
    oe_fifo_t fifo;
    uint32_t expected;
    oe_result_t read_res;
    uint32_t read_value;
} fifo_test_ctx_t;

static void fifo_reader_worker(void *arg)
{
    fifo_test_ctx_t *ctx = (fifo_test_ctx_t *)arg;
    oe_result_t r;
    size_t got = 0;
    uint32_t v = 0;

    r = oe_fifo_read(&ctx->fifo, &v, sizeof(v), &got, -1);
    ctx->read_res = r;
    ctx->read_value = v;
}

MU_TEST(test_fifo_read_write)
{
    fifo_test_ctx_t ctx;
    oe_result_t r;
    oe_thread_t th;
    char path[128];

    snprintf(path, sizeof(path), "/tmp/oe_fifo_test_%d_%u.fifo", (int)getpid(), (unsigned)rand());
    (void)oe_fifo_unlink(path);

    memset(&ctx, 0, sizeof(ctx));
    ctx.expected = 0x12345678u;

    /* Open fifo for reading inside worker thread context */
    r = oe_fifo_open(&ctx.fifo, path, OE_FIFO_MODE_READ);
    mu_assert(r == OE_OK, "fifo open read");

    r = oe_thread_create(&th, fifo_reader_worker, &ctx);
    mu_assert(r == OE_OK, "fifo reader thread create");

    oe_fifo_t writer;
    memset(&writer, 0, sizeof(writer));
    r = oe_fifo_open(&writer, path, OE_FIFO_MODE_WRITE);
    mu_assert(r == OE_OK, "fifo open write");

    size_t sent = 0;
    r = oe_fifo_write(&writer, &ctx.expected, sizeof(ctx.expected), &sent, 1000);
    mu_assert(r == OE_OK, "fifo write");

    r = oe_thread_join(&th);
    mu_assert(r == OE_OK, "fifo reader thread join");

    mu_assert(ctx.read_res == OE_OK, "fifo read ok");
    mu_assert(ctx.read_value == ctx.expected, "fifo read value match");

    r = oe_fifo_close(&writer);
    mu_assert(r == OE_OK, "fifo writer close");

    r = oe_fifo_close(&ctx.fifo);
    mu_assert(r == OE_OK, "fifo reader close");

    r = oe_fifo_unlink(path);
    mu_assert(r == OE_OK, "fifo unlink");
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
    oe_signals_t sigs;
    oe_signals_caps_t caps;
    oe_signal_info_t info;
    oe_result_t r;

    int signum = SIGUSR1;
    memset(&sigs, 0, sizeof(sigs));
    memset(&caps, 0, sizeof(caps));
    memset(&info, 0, sizeof(info));

    r = oe_signals_query_caps(&caps);
    mu_assert(r == OE_OK, "signals query caps");
    mu_check(caps.supports_signalfd == 1u);

    r = oe_signals_open(&sigs, &signum, 1);
    mu_assert(r == OE_OK, "signals open");

    r = oe_signals_wait(&sigs, &info, 10);
    mu_assert(r == OE_ERR_TIMEOUT, "signals wait timeout");

    /* Send one SIGUSR1 from another thread. */
    signals_sender_ctx_t sctx;
    oe_thread_t th;
    memset(&sctx, 0, sizeof(sctx));
    sctx.signum = signum;

    r = oe_thread_create(&th, signals_sender_worker, &sctx);
    mu_assert(r == OE_OK, "signals sender thread create");

    r = oe_signals_wait(&sigs, &info, 1000);
    mu_assert(r == OE_OK, "signals wait ok");
    mu_assert(info.signum == signum, "signals wait signum match");

    r = oe_thread_join(&th);
    mu_assert(r == OE_OK, "signals sender thread join");

    r = oe_signals_close(&sigs);
    mu_assert(r == OE_OK, "signals close");
}

MU_TEST(test_mq_send_recv_optional)
{
    oe_mq_t mq;
    oe_result_t r;
    oe_mq_caps_t caps;
    char name[128];
    char send_buf[32];
    char recv_buf[32];
    size_t recv_len = 0;
    const char *msg = "oe_mq_test";
    size_t msg_len = strlen(msg);

    snprintf(name, sizeof(name), "/oe_mq_test_%d_%u", (int)getpid(), (unsigned)rand());
    (void)oe_mq_unlink(name);

    r = oe_mq_query_caps(&caps);
    mu_assert(r == OE_OK, "mq query caps");
    mu_check(caps.supports_message_queue == 1u);

    memset(&mq, 0, sizeof(mq));
    r = oe_mq_create(&mq, name, 10, sizeof(send_buf));
    if (r == OE_ERR_UNSUPPORTED) {
        /* Environment may not support POSIX mqueue (e.g. /dev/mqueue not available) */
        return;
    }
    mu_assert(r == OE_OK, "mq create");

    memset(send_buf, 0, sizeof(send_buf));
    memcpy(send_buf, msg, msg_len);

    r = oe_mq_send(&mq, send_buf, msg_len, 1000);
    mu_assert(r == OE_OK, "mq send");

    memset(recv_buf, 0, sizeof(recv_buf));
    r = oe_mq_recv(&mq, recv_buf, sizeof(recv_buf), &recv_len, 1000);
    mu_assert(r == OE_OK, "mq recv");
    mu_assert(recv_len == msg_len, "mq recv length match");
    mu_assert(memcmp(recv_buf, msg, msg_len) == 0, "mq message match");

    r = oe_mq_close(&mq);
    mu_assert(r == OE_OK, "mq close");
    r = oe_mq_unlink(name);
    mu_assert(r == OE_OK, "mq unlink");
}

MU_TEST(test_socket_tcp_send_recv)
{
    oe_result_t r;
    oe_socket_t server;
    oe_socket_t client;
    oe_socket_t accepted;
    uint16_t port = 0;

    memset(&server, 0, sizeof(server));
    memset(&client, 0, sizeof(client));
    memset(&accepted, 0, sizeof(accepted));

    r = oe_socket_open_tcp_server(&server, "127.0.0.1", 0, 8);
    mu_assert(r == OE_OK, "tcp server open");

    r = oe_socket_get_local_port(&server, &port);
    mu_assert(r == OE_OK, "tcp server local port");
    mu_check(port != 0);

    r = oe_socket_open_tcp_client(&client, "localhost", port, 1000);
    mu_assert(r == OE_OK, "tcp client open");

    r = oe_socket_accept(&server, &accepted, 1000);
    mu_assert(r == OE_OK, "tcp accept");

    uint32_t req = 0x11111111u;
    uint32_t resp = 0x22222222u;
    size_t sent = 0;
    size_t recvd = 0;

    r = oe_socket_send(&client, &req, sizeof(req), &sent, 1000);
    mu_assert(r == OE_OK, "tcp client send");

    uint32_t got_req = 0;
    r = oe_socket_recv(&accepted, &got_req, sizeof(got_req), &recvd, 1000);
    mu_assert(r == OE_OK, "tcp server recv");
    mu_assert(got_req == req, "tcp server req match");

    r = oe_socket_send(&accepted, &resp, sizeof(resp), &sent, 1000);
    mu_assert(r == OE_OK, "tcp server send");

    uint32_t got_resp = 0;
    r = oe_socket_recv(&client, &got_resp, sizeof(got_resp), &recvd, 1000);
    mu_assert(r == OE_OK, "tcp client recv");
    mu_assert(got_resp == resp, "tcp client resp match");

    (void)oe_socket_close(&accepted);
    (void)oe_socket_close(&client);
    (void)oe_socket_close(&server);
}

MU_TEST(test_socket_udp_send_recv)
{
    oe_result_t r;
    oe_socket_t rx;
    oe_socket_t tx;
    uint16_t port = 0;

    memset(&rx, 0, sizeof(rx));
    memset(&tx, 0, sizeof(tx));

    r = oe_socket_open_udp(&rx, "127.0.0.1", 0);
    mu_assert(r == OE_OK, "udp rx open");

    r = oe_socket_get_local_port(&rx, &port);
    mu_assert(r == OE_OK, "udp rx local port");
    mu_check(port != 0);

    r = oe_socket_open_udp(&tx, "127.0.0.1", 0);
    mu_assert(r == OE_OK, "udp tx open");

    r = oe_socket_udp_connect(&tx, "127.0.0.1", port);
    mu_assert(r == OE_OK, "udp connect");

    const char msg[] = "hello-udp";
    size_t sent = 0;
    r = oe_socket_send(&tx, msg, sizeof(msg), &sent, 1000);
    mu_assert(r == OE_OK, "udp send");

    char buf[32];
    size_t got = 0;
    memset(buf, 0, sizeof(buf));
    r = oe_socket_recv(&rx, buf, sizeof(buf), &got, 1000);
    mu_assert(r == OE_OK, "udp recv");
    mu_check(got > 0);
    mu_assert(memcmp(buf, msg, sizeof(msg)) == 0, "udp payload match");

    (void)oe_socket_close(&tx);
    (void)oe_socket_close(&rx);
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
