#include "minunit.h"

#include "openember/algorithm/algorithm.hpp"

#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>

namespace algo = openember::algorithm;

static bool near(double a, double b, double eps = 1e-9)
{
    return std::fabs(a - b) <= eps;
}

MU_TEST(test_checksums)
{
    const char *data = "123456789";
    const auto size = std::strlen(data);

    mu_assert_int_eq(0xF4, algo::crc8(data, size));
    mu_assert_int_eq(0x4B37, algo::crc16_modbus(data, size));
    mu_assert_int_eq(0x29B1, algo::crc16_ccitt_false(data, size));
    mu_check(algo::crc32(data, size) == 0xCBF43926u);

    const std::uint8_t bytes[] = {0x01, 0x02, 0x03};
    mu_assert_int_eq(0x00, algo::xor_checksum(bytes, sizeof(bytes)));
    mu_assert_int_eq(0x06, algo::sum8(bytes, sizeof(bytes)));
}

MU_TEST(test_filters)
{
    algo::MovingAverage<double, 3> avg;
    mu_check(near(avg.update(1.0), 1.0));
    mu_check(near(avg.update(2.0), 1.5));
    mu_check(near(avg.update(3.0), 2.0));
    mu_check(near(avg.update(7.0), 4.0));

    algo::ExponentialMovingAverage<double> ema(0.5);
    mu_check(near(ema.update(10.0), 10.0));
    mu_check(near(ema.update(14.0), 12.0));

    algo::MedianFilter<int, 3> median;
    mu_assert_int_eq(10, median.update(10));
    mu_assert_int_eq(10, median.update(100));
    mu_assert_int_eq(10, median.update(0));

    algo::OutlierRejector<double> rejector(5.0);
    mu_check(near(rejector.update(10.0), 10.0));
    mu_check(near(rejector.update(100.0), 10.0));
    mu_check(near(rejector.update(12.0), 12.0));
}

MU_TEST(test_array_support)
{
    algo::MovingAverage<std::array<double, 3>, 2> avg;
    auto value = avg.update({1.0, 2.0, 3.0});
    mu_check(near(value[0], 1.0));
    mu_check(near(value[1], 2.0));
    mu_check(near(value[2], 3.0));

    value = avg.update({3.0, 4.0, 5.0});
    mu_check(near(value[0], 2.0));
    mu_check(near(value[1], 3.0));
    mu_check(near(value[2], 4.0));
}

MU_TEST(test_statistics)
{
    algo::SlidingWindow<double, 3> window;
    window.push(1.0);
    window.push(2.0);
    window.push(6.0);
    mu_check(near(window.mean(), 3.0));
    window.push(7.0);
    mu_check(near(window.mean(), 5.0));

    algo::MeanVariance stats;
    stats.update(1.0);
    stats.update(2.0);
    stats.update(3.0);
    mu_check(near(stats.mean(), 2.0));
    mu_check(near(stats.variance(), 1.0));

    algo::FrequencyEstimator frequency;
    mu_check(near(frequency.update(10.0), 0.0));
    mu_check(near(frequency.update(10.5), 2.0));
}

MU_TEST(test_control_and_signal)
{
    algo::Saturation<double> saturation(-1.0, 1.0);
    mu_check(near(saturation.update(2.5), 1.0));

    algo::Deadband<double> deadband(0.1);
    mu_check(near(deadband.update(0.05), 0.0));
    mu_check(near(deadband.update(0.2), 0.2));

    algo::PID<double> pid({1.0, 0.1, 0.0}, -10.0, 10.0);
    mu_check(pid.update(10.0, 0.0, 0.1) <= 10.0);

    algo::Hysteresis hysteresis(0.3, 0.7);
    mu_check(!hysteresis.update(0.6));
    mu_check(hysteresis.update(0.8));
    mu_check(hysteresis.update(0.4));
    mu_check(!hysteresis.update(0.2));

    algo::EdgeDetector edge;
    mu_check(edge.rising(true));
    mu_check(!edge.rising(true));
}

MU_TEST_SUITE(test_suite)
{
    MU_RUN_TEST(test_checksums);
    MU_RUN_TEST(test_filters);
    MU_RUN_TEST(test_array_support);
    MU_RUN_TEST(test_statistics);
    MU_RUN_TEST(test_control_and_signal);
}

int main()
{
    MU_RUN_SUITE(test_suite);
    MU_REPORT();
    return MU_EXIT_CODE;
}
