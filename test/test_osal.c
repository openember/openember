#include "minunit.h"

#include "openember/osal/mutex.h"
#include "openember/osal/thread.h"
#include "openember/osal/time.h"
#include "openember/osal/types.h"

#include <stdint.h>

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

MU_TEST_SUITE(osal_suite)
{
    MU_SUITE_CONFIGURE(NULL, NULL);
    MU_RUN_TEST(test_mutex_lock_unlock);
    MU_RUN_TEST(test_thread_join);
    MU_RUN_TEST(test_time_monotonic);
}

int main(void)
{
    MU_RUN_SUITE(osal_suite);
    MU_REPORT();
    return minunit_fail > 0;
}
