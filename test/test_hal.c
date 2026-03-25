#define _POSIX_C_SOURCE 200809L

#include "minunit.h"

#include "openember/hal/file.h"
#include "openember/hal/uart.h"
#include "openember/osal/types.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

MU_TEST(test_hal_file_roundtrip)
{
    oe_file_t f;
    char path[] = "/tmp/oe_hal_test_XXXXXX";
    char buf[64];
    size_t n = 0;
    oe_result_t r;
    int fd;

    fd = mkstemp(path);
    mu_assert(fd >= 0, "mkstemp");
    close(fd);
    unlink(path);

    r = oe_file_open(&f, path, "w+");
    mu_assert(r == OE_OK, "open w+");

    r = oe_file_write(&f, "hello", 5, &n);
    mu_assert(r == OE_OK, "write");
    mu_assert_int_eq(5, (int)n);

    r = oe_file_flush(&f);
    mu_assert(r == OE_OK, "flush");

    r = oe_file_seek(&f, 0, OE_FILE_SEEK_SET, NULL);
    mu_assert(r == OE_OK, "seek set");

    memset(buf, 0, sizeof(buf));
    r = oe_file_read(&f, buf, sizeof(buf) - 1, &n);
    mu_assert(r == OE_OK, "read");
    mu_assert_int_eq(5, (int)n);
    mu_assert_string_eq("hello", buf);

    r = oe_file_close(&f);
    mu_assert(r == OE_OK, "close");

    unlink(path);
}

MU_TEST(test_hal_uart_open_invalid_path)
{
    oe_uart_t u;
    oe_uart_config_t cfg = {
        .baud_rate = 115200,
        .data_bits = 8,
        .stop_bits = 1,
        .parity = OE_UART_PARITY_NONE,
    };
    oe_result_t r;

    r = oe_uart_open(&u, "/dev/nonexistent_openember_uart", &cfg);
    mu_assert(r != OE_OK, "expect failure");
}

MU_TEST(test_hal_caps)
{
    oe_file_caps_t fc;
    oe_uart_caps_t uc;
    oe_result_t r;

    memset(&fc, 0, sizeof(fc));
    memset(&uc, 0, sizeof(uc));

    r = oe_file_query_caps(&fc);
    mu_assert(r == OE_OK, "file query caps");
    mu_check(fc.flags != 0);

    r = oe_uart_query_caps(&uc);
    mu_assert(r == OE_OK, "uart query caps");
    mu_check(uc.baud_rate_count > 0);
    mu_check(uc.parity_mask != 0);
}

MU_TEST_SUITE(hal_suite)
{
    MU_SUITE_CONFIGURE(NULL, NULL);
    MU_RUN_TEST(test_hal_file_roundtrip);
    MU_RUN_TEST(test_hal_uart_open_invalid_path);
    MU_RUN_TEST(test_hal_caps);
}

int main(void)
{
    MU_RUN_SUITE(hal_suite);
    MU_REPORT();
    return minunit_fail > 0;
}
