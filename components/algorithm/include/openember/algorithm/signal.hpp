#ifndef OPENEMBER_ALGORITHM_SIGNAL_HPP_
#define OPENEMBER_ALGORITHM_SIGNAL_HPP_

#include "openember/algorithm/control.hpp"

namespace openember::algorithm {

template <typename T>
constexpr T map_range(T value, T in_min, T in_max, T out_min, T out_max)
{
    return static_cast<T>((value - in_min) * (out_max - out_min) / (in_max - in_min) + out_min);
}

template <typename T>
constexpr T normalize(T value, T min_value, T max_value)
{
    return map_range(value, min_value, max_value, static_cast<T>(0), static_cast<T>(1));
}

template <typename T>
constexpr T rescale(T normalized, T min_value, T max_value)
{
    return map_range(normalized, static_cast<T>(0), static_cast<T>(1), min_value, max_value);
}

template <typename T>
constexpr bool threshold(T value, T threshold_value)
{
    return value >= threshold_value;
}

class EdgeDetector {
public:
    bool rising(bool value)
    {
        const bool edge = value && !previous_;
        previous_ = value;
        return edge;
    }

    bool falling(bool value)
    {
        const bool edge = !value && previous_;
        previous_ = value;
        return edge;
    }

    void reset(bool value = false) { previous_ = value; }

private:
    bool previous_{false};
};

}  // namespace openember::algorithm

#endif  // OPENEMBER_ALGORITHM_SIGNAL_HPP_
