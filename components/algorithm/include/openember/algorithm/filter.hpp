#ifndef OPENEMBER_ALGORITHM_FILTER_HPP_
#define OPENEMBER_ALGORITHM_FILTER_HPP_

#include "openember/algorithm/detail/math.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <type_traits>

namespace openember::algorithm {

template <typename T, std::size_t Window>
class MovingAverage {
    static_assert(Window > 0, "MovingAverage window must be greater than zero");

public:
    T update(const T &sample)
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
        return value();
    }

    T value() const
    {
        return count_ == 0 ? detail::zero<T>() : detail::divide(sum_, count_);
    }

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
class ExponentialMovingAverage {
public:
    explicit ExponentialMovingAverage(double alpha)
        : alpha_(std::clamp(alpha, 0.0, 1.0)) {}

    T update(const T &sample)
    {
        if (!initialized_) {
            value_ = sample;
            initialized_ = true;
            return value_;
        }

        value_ = detail::add(detail::scale(sample, alpha_),
                             detail::scale(value_, 1.0 - alpha_));
        return value_;
    }

    const T &value() const { return value_; }
    bool initialized() const { return initialized_; }

    void reset()
    {
        value_ = detail::zero<T>();
        initialized_ = false;
    }

private:
    double alpha_{1.0};
    T value_{};
    bool initialized_{false};
};

template <typename T>
class LowPassFilter {
public:
    LowPassFilter(double cutoff_hz, double sample_period_s)
        : alpha_(compute_alpha(cutoff_hz, sample_period_s)), ema_(alpha_) {}

    T update(const T &sample) { return ema_.update(sample); }
    const T &value() const { return ema_.value(); }
    void reset() { ema_.reset(); }

private:
    static double compute_alpha(double cutoff_hz, double sample_period_s)
    {
        if (cutoff_hz <= 0.0 || sample_period_s <= 0.0) {
            return 1.0;
        }

        constexpr double kPi = 3.14159265358979323846;
        const double rc = 1.0 / (2.0 * kPi * cutoff_hz);
        return sample_period_s / (rc + sample_period_s);
    }

    double alpha_;
    ExponentialMovingAverage<T> ema_;
};

template <typename T, std::size_t Window>
class MedianFilter {
    static_assert(Window > 0, "MedianFilter window must be greater than zero");
    static_assert(!detail::is_std_array_v<T>, "MedianFilter supports scalar types only");

public:
    T update(const T &sample)
    {
        values_[next_] = sample;
        next_ = (next_ + 1) % Window;
        if (count_ < Window) {
            ++count_;
        }

        auto sorted = values_;
        std::sort(sorted.begin(), sorted.begin() + static_cast<std::ptrdiff_t>(count_));
        return sorted[(count_ - 1) / 2];
    }

    std::size_t size() const { return count_; }

    void reset()
    {
        values_ = {};
        count_ = 0;
        next_ = 0;
    }

private:
    std::array<T, Window> values_{};
    std::size_t count_{0};
    std::size_t next_{0};
};

template <typename T>
class OutlierRejector {
public:
    OutlierRejector(double max_delta, const T &initial = T{})
        : max_delta_(max_delta), value_(initial) {}

    T update(const T &sample)
    {
        if (!initialized_) {
            value_ = sample;
            initialized_ = true;
            return value_;
        }

        const auto delta = detail::sub(sample, value_);
        if (detail::magnitude(delta) <= max_delta_) {
            value_ = sample;
        }
        return value_;
    }

    const T &value() const { return value_; }
    void reset() { initialized_ = false; }

private:
    double max_delta_{0.0};
    T value_{};
    bool initialized_{false};
};

class Hysteresis {
public:
    Hysteresis(double low_threshold, double high_threshold)
        : low_(low_threshold), high_(high_threshold) {}

    bool update(double value)
    {
        if (state_) {
            state_ = value > low_;
        } else {
            state_ = value >= high_;
        }
        return state_;
    }

    bool state() const { return state_; }
    void reset(bool state = false) { state_ = state; }

private:
    double low_{0.0};
    double high_{0.0};
    bool state_{false};
};

}  // namespace openember::algorithm

#endif  // OPENEMBER_ALGORITHM_FILTER_HPP_
