#include "minunit.h"

#include "openember/osal/mutex.h"
#include "openember/osal/cond.h"
#include "openember/osal/event.h"
#include "openember/osal/sem.h"
#include "openember/osal/shm.h"
#include "openember/osal/thread.h"
#include "openember/osal/time.h"
#include "openember/osal/types.h"

#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>

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
}

int main(void)
{
    MU_RUN_SUITE(osal_suite);
    MU_REPORT();
    return minunit_fail > 0;
}
