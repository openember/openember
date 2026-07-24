#ifndef OPENEMBER_ALGORITHM_DETAIL_MATH_HPP_
#define OPENEMBER_ALGORITHM_DETAIL_MATH_HPP_

#include <array>
#include <cmath>
#include <cstddef>
#include <type_traits>

namespace openember::algorithm::detail {

template <typename T>
struct is_std_array : std::false_type {};

template <typename T, std::size_t N>
struct is_std_array<std::array<T, N>> : std::true_type {};

template <typename T>
inline constexpr bool is_std_array_v = is_std_array<T>::value;

template <typename T>
constexpr T zero()
{
    if constexpr (is_std_array_v<T>) {
        T out{};
        for (auto &v : out) {
            v = typename T::value_type{};
        }
        return out;
    } else {
        return T{};
    }
}

template <typename T>
constexpr T add(const T &a, const T &b)
{
    if constexpr (is_std_array_v<T>) {
        T out{};
        for (std::size_t i = 0; i < out.size(); ++i) {
            out[i] = a[i] + b[i];
        }
        return out;
    } else {
        return a + b;
    }
}

template <typename T>
constexpr T sub(const T &a, const T &b)
{
    if constexpr (is_std_array_v<T>) {
        T out{};
        for (std::size_t i = 0; i < out.size(); ++i) {
            out[i] = a[i] - b[i];
        }
        return out;
    } else {
        return a - b;
    }
}

template <typename T, typename Scalar>
constexpr T scale(const T &value, Scalar scalar)
{
    if constexpr (is_std_array_v<T>) {
        T out{};
        for (std::size_t i = 0; i < out.size(); ++i) {
            out[i] = static_cast<typename T::value_type>(value[i] * scalar);
        }
        return out;
    } else {
        return static_cast<T>(value * scalar);
    }
}

template <typename T, typename Scalar>
constexpr T divide(const T &value, Scalar scalar)
{
    return scale(value, static_cast<double>(1.0) / static_cast<double>(scalar));
}

template <typename T>
constexpr T clamp_value(const T &value, const T &low, const T &high)
{
    return value < low ? low : (high < value ? high : value);
}

template <typename T>
constexpr T clamp_delta(const T &delta, const T &limit)
{
    return clamp_value(delta, static_cast<T>(-limit), limit);
}

template <typename T>
inline double magnitude(const T &value)
{
    if constexpr (is_std_array_v<T>) {
        double sum = 0.0;
        for (const auto &v : value) {
            const double x = static_cast<double>(v);
            sum += x * x;
        }
        return std::sqrt(sum);
    } else {
        return std::fabs(static_cast<double>(value));
    }
}

template <typename T>
inline bool approximately_zero(const T &value, double epsilon)
{
    return magnitude(value) <= epsilon;
}

}  // namespace openember::algorithm::detail

#endif  // OPENEMBER_ALGORITHM_DETAIL_MATH_HPP_
