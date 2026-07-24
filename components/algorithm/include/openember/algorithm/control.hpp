#ifndef OPENEMBER_ALGORITHM_CONTROL_HPP_
#define OPENEMBER_ALGORITHM_CONTROL_HPP_

#include "openember/algorithm/detail/math.hpp"

#include <algorithm>

namespace openember::algorithm {

template <typename T>
constexpr T clamp(const T &value, const T &low, const T &high)
{
    return detail::clamp_value(value, low, high);
}

template <typename T>
class Saturation {
public:
    Saturation(T low, T high) : low_(low), high_(high) {}

    T update(T value) const { return clamp(value, low_, high_); }

private:
    T low_;
    T high_;
};

template <typename T>
class Deadband {
public:
    explicit Deadband(T threshold) : threshold_(threshold) {}

    T update(T value) const
    {
        return detail::magnitude(value) <= static_cast<double>(threshold_) ? T{} : value;
    }

private:
    T threshold_;
};

template <typename T>
class SlewRateLimiter {
public:
    SlewRateLimiter(T rising_rate, T falling_rate)
        : rising_rate_(rising_rate), falling_rate_(falling_rate) {}

    explicit SlewRateLimiter(T rate) : SlewRateLimiter(rate, rate) {}

    T update(T target, double dt_s)
    {
        if (!initialized_) {
            value_ = target;
            initialized_ = true;
            return value_;
        }

        const T delta = target - value_;
        const T up = static_cast<T>(rising_rate_ * dt_s);
        const T down = static_cast<T>(falling_rate_ * dt_s);
        const T limited = delta >= T{} ? std::min(delta, up) : std::max(delta, static_cast<T>(-down));
        value_ = static_cast<T>(value_ + limited);
        return value_;
    }

    void reset(T value = T{})
    {
        value_ = value;
        initialized_ = true;
    }

    T value() const { return value_; }

private:
    T rising_rate_;
    T falling_rate_;
    T value_{};
    bool initialized_{false};
};

template <typename T>
class PID {
public:
    struct Gains {
        T kp{};
        T ki{};
        T kd{};
    };

    PID(Gains gains, T output_min, T output_max)
        : gains_(gains), output_min_(output_min), output_max_(output_max) {}

    T update(T setpoint, T measurement, double dt_s)
    {
        const T error = static_cast<T>(setpoint - measurement);
        if (dt_s <= 0.0) {
            return clamp(static_cast<T>(gains_.kp * error + gains_.ki * integral_), output_min_, output_max_);
        }

        integral_ = static_cast<T>(integral_ + error * dt_s);
        const T derivative = has_previous_ ? static_cast<T>((error - previous_error_) / dt_s) : T{};
        previous_error_ = error;
        has_previous_ = true;

        const T output = static_cast<T>(gains_.kp * error + gains_.ki * integral_ + gains_.kd * derivative);
        const T saturated = clamp(output, output_min_, output_max_);

        if (output != saturated && gains_.ki != T{}) {
            integral_ = static_cast<T>((saturated - gains_.kp * error - gains_.kd * derivative) / gains_.ki);
        }

        return saturated;
    }

    void reset()
    {
        integral_ = T{};
        previous_error_ = T{};
        has_previous_ = false;
    }

    T integral() const { return integral_; }

private:
    Gains gains_;
    T output_min_;
    T output_max_;
    T integral_{};
    T previous_error_{};
    bool has_previous_{false};
};

}  // namespace openember::algorithm

#endif  // OPENEMBER_ALGORITHM_CONTROL_HPP_
