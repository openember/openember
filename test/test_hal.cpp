#define _POSIX_C_SOURCE 200809L

#include "minunit.h"

#include "openember/hal/hal.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>

using openember::hal::Can;
using openember::hal::CanCaps;
using openember::hal::File;
using openember::hal::FileCaps;
using openember::hal::FileWhence;
using openember::hal::Gpio;
using openember::hal::GpioCaps;
using openember::hal::I2c;
using openember::hal::I2cCaps;
using openember::hal::Onewire;
using openember::hal::OnewireCaps;
using openember::hal::Result;
using openember::hal::Sbus;
using openember::hal::SbusCaps;
using openember::hal::Spi;
using openember::hal::SpiCaps;
using openember::hal::SerialBaud;
using openember::hal::SerialParity;
using openember::hal::SerialPort;
using openember::hal::SerialPortCaps;
using openember::hal::SerialPortConfig;
using openember::hal::is_standard_serial_baud;
using openember::osal::kOk;

MU_TEST(test_hal_file_roundtrip)
{
    File f;
    char path[] = "/tmp/oe_hal_test_XXXXXX";
    char buf[64];
    size_t n = 0;
    Result r;
    const int fd = mkstemp(path);
    mu_assert(fd >= 0, "mkstemp");
    ::close(fd);
    unlink(path);

    r = f.open(path, "w+");
    mu_assert(r == kOk, "open w+");

    r = f.write("hello", 5, &n);
    mu_assert(r == kOk, "write");
    mu_assert_int_eq(5, static_cast<int>(n));

    r = f.flush();
    mu_assert(r == kOk, "flush");

    r = f.seek(0, FileWhence::Set);
    mu_assert(r == kOk, "seek set");

    std::memset(buf, 0, sizeof(buf));
    r = f.read(buf, sizeof(buf) - 1, &n);
    mu_assert(r == kOk, "read");
    mu_assert_int_eq(5, static_cast<int>(n));
    mu_assert_string_eq("hello", buf);

    r = f.close();
    mu_assert(r == kOk, "close");

    unlink(path);
}

MU_TEST(test_hal_serial_port_open_invalid_path)
{
    SerialPort sp;
    auto cfg = SerialPortConfig::with_baud(SerialBaud::B115200);
    cfg.parity = SerialParity::None;

    const Result r = sp.open("/dev/nonexistent_openember_uart", cfg);
    mu_assert(r != kOk, "expect failure");
}

MU_TEST(test_hal_serial_port_arbitrary_baud_flag)
{
    mu_check(!is_standard_serial_baud(100000));
    mu_check(is_standard_serial_baud(static_cast<uint32_t>(SerialBaud::B115200)));
}

MU_TEST(test_hal_caps)
{
    FileCaps fc {};
    SerialPortCaps uc {};
    GpioCaps gc {};
    Result r;

    r = File::query_caps(&fc);
    mu_assert(r == kOk, "file query caps");
    mu_check(fc.flags != 0);

    r = SerialPort::query_caps(&uc);
    mu_assert(r == kOk, "serial port query caps");
    mu_check(uc.baud_rate_count > 0);
    mu_check(uc.parity_mask != 0);

    r = Gpio::query_caps(&gc);
    mu_assert(r == kOk, "gpio query caps");
    mu_check(gc.direction_mask != 0);

    {
        I2cCaps ic {};
        r = I2c::query_caps(&ic);
        mu_assert(r == kOk, "i2c query caps");
        mu_check(ic.supports_write_read == 1u);
    }

    {
        SpiCaps sc {};
        r = Spi::query_caps(&sc);
        mu_assert(r == kOk, "spi query caps");
        mu_check(sc.supports_full_duplex == 1u);
    }

    {
        CanCaps cc {};
        r = Can::query_caps(&cc);
        mu_assert(r == kOk, "can query caps");
        mu_check(cc.max_data_len_can == 8u);
        mu_check(cc.max_data_len_can_fd <= 64u);
    }

    {
        SbusCaps sc {};
        r = Sbus::query_caps(&sc);
        mu_assert(r == kOk, "sbus query caps");
        mu_check(sc.channels_count == 16u);
        mu_check(sc.switches_count == 2u);
        mu_check(sc.frame_len_bytes == 25u);
        mu_check(sc.baud_rate == 100000u);
    }

    {
        OnewireCaps oc {};
        r = Onewire::query_caps(&oc);
        mu_assert(r == kOk, "onewire query caps");
        mu_check(oc.supports_temperature == 1u);
        mu_check(oc.supports_raw_read == 1u);
    }
}

MU_TEST_SUITE(hal_suite)
{
    MU_SUITE_CONFIGURE(nullptr, nullptr);
    MU_RUN_TEST(test_hal_file_roundtrip);
    MU_RUN_TEST(test_hal_serial_port_open_invalid_path);
    MU_RUN_TEST(test_hal_serial_port_arbitrary_baud_flag);
    MU_RUN_TEST(test_hal_caps);
}

int main()
{
    MU_RUN_SUITE(hal_suite);
    MU_REPORT();
    return minunit_fail > 0;
}
