#ifndef OPENEMBER_ALGORITHM_STATISTICS_HPP_
#define OPENEMBER_ALGORITHM_STATISTICS_HPP_

#include "openember/algorithm/detail/math.hpp"

#include <array>
#include <cstddef>
#include <limits>

namespace openember::algorithm {

template <typename T, std::size_t Window>
class SlidingWindow {
    static_assert(Window > 0, "SlidingWindow window must be greater than zero");

public:
    void push(const T &sample)
    {
        if (count_ < Window) {
            values_[count_++] = sample;
            sum_ = detail::add(sum_, sample);
        } else {
            sum_ = detail::sub(sum_, values_[next_]);
            values_[next_] = sample;
            sum_ = detail::add(sum_, sample);
        }
        next_ = (next_ + 1) % Window;
    }

    T mean() const
    {
        return count_ == 0 ? detail::zero<T>() : detail::divide(sum_, count_);
    }

    const T &sum() const { return sum_; }
    std::size_t size() const { return count_; }
    bool full() const { return count_ == Window; }

    void reset()
    {
        values_ = {};
        sum_ = detail::zero<T>();
        count_ = 0;
        next_ = 0;
    }

private:
    std::array<T, Window> values_{};
    T sum_{};
    std::size_t count_{0};
    std::size_t next_{0};
};

template <typename T>
class MinMax {
public:
    void update(T sample)
    {
        if (!initialized_) {
            min_ = sample;
            max_ = sample;
            initialized_ = true;
            return;
        }
        if (sample < min_) {
            min_ = sample;
        }
        if (max_ < sample) {
            max_ = sample;
        }
    }

    T min() const { return min_; }
    T max() const { return max_; }
    bool initialized() const { return initialized_; }

    void reset()
    {
        min_ = T{};
        max_ = T{};
        initialized_ = false;
    }

private:
    T min_{};
    T max_{};
    bool initialized_{false};
};

class MeanVariance {
public:
    void update(double sample)
    {
        ++count_;
        const double delta = sample - mean_;
        mean_ += delta / static_cast<double>(count_);
        const double delta2 = sample - mean_;
        m2_ += delta * delta2;
    }

    std::size_t count() const { return count_; }
    double mean() const { return mean_; }
    double variance() const { return count_ > 1 ? m2_ / static_cast<double>(count_ - 1) : 0.0; }

    void reset()
    {
        count_ = 0;
        mean_ = 0.0;
        m2_ = 0.0;
    }

private:
    std::size_t count_{0};
    double mean_{0.0};
    double m2_{0.0};
};

class RateCounter {
public:
    void add(std::size_t count = 1) { count_ += count; }

    double rate(double elapsed_s) const
    {
        return elapsed_s > 0.0 ? static_cast<double>(count_) / elapsed_s : 0.0;
    }

    std::size_t count() const { return count_; }
    void reset() { count_ = 0; }

private:
    std::size_t count_{0};
};

class FrequencyEstimator {
public:
    double update(double timestamp_s)
    {
        if (!initialized_) {
            last_timestamp_s_ = timestamp_s;
            initialized_ = true;
            return 0.0;
        }

        const double dt = timestamp_s - last_timestamp_s_;
        last_timestamp_s_ = timestamp_s;
        frequency_hz_ = dt > 0.0 ? 1.0 / dt : 0.0;
        return frequency_hz_;
    }

    double frequency_hz() const { return frequency_hz_; }
    void reset() { initialized_ = false; }

private:
    double last_timestamp_s_{0.0};
    double frequency_hz_{0.0};
    bool initialized_{false};
};

}  // namespace openember::algorithm

#endif  // OPENEMBER_ALGORITHM_STATISTICS_HPP_
