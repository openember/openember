#pragma once

#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace openember::config {
enum class ErrorCode { kIo, kParse, kLimit, kDuplicateKey, kMissing, kType, kInvalidValue, kUnknownKey };
class ConfigError : public std::runtime_error {
 public:
    ConfigError(ErrorCode code, std::string source, std::string path, std::string message,
                int line = 0, int column = 0);
    ErrorCode Code() const noexcept { return code_; }
    const std::string& Source() const noexcept { return source_; }
    const std::string& Path() const noexcept { return path_; }
    int Line() const noexcept { return line_; }
    int Column() const noexcept { return column_; }
 private:
    ErrorCode code_;
    std::string source_, path_;
    int line_, column_;
};
struct LoadOptions {
    std::size_t max_bytes = 1024 * 1024;
    std::size_t max_depth = 32;
    std::size_t max_nodes = 65536;
};
enum class Kind { kNull, kScalar, kSequence, kMap };
namespace detail {
struct Value;
template <class T> struct IsVector : std::false_type {};
template <class T, class A> struct IsVector<std::vector<T, A>> : std::true_type { using Element = T; };
}
// Immutable snapshot, independent of the YAML parser's nodes and lifetime.
class Node {
 public:
    Kind Type() const noexcept;
    const std::string& Source() const noexcept { return source_; }
    const std::string& Path() const noexcept { return path_; }
    int Line() const noexcept;
    int Column() const noexcept;
    Node At(std::string_view key) const;
    Node At(std::size_t index) const;
    std::optional<Node> Find(std::string_view key) const;
    std::size_t Size() const;
    std::vector<std::string> Keys() const;
    void CheckKeys(std::initializer_list<std::string_view> allowed) const;
    void Require(bool condition, const std::string& message) const;
    [[noreturn]] void Fail(const std::string& message, ErrorCode code = ErrorCode::kInvalidValue) const;

    template <class T> T As() const {
        if constexpr (std::is_same_v<T, std::string>) return Scalar();
        else if constexpr (std::is_same_v<T, bool>) return Boolean();
        else if constexpr (std::is_integral_v<T> && std::is_signed_v<T>) {
            const auto value = Signed();
            if (value < std::numeric_limits<T>::min() || value > std::numeric_limits<T>::max())
                Fail("integer out of range");
            return static_cast<T>(value);
        } else if constexpr (std::is_integral_v<T> && std::is_unsigned_v<T>) {
            const auto value = Unsigned();
            if (value > std::numeric_limits<T>::max()) Fail("unsigned integer out of range");
            return static_cast<T>(value);
        } else if constexpr (std::is_floating_point_v<T>) {
            const auto value = Number();
            if (value < -std::numeric_limits<T>::max() || value > std::numeric_limits<T>::max())
                Fail("floating point value out of range");
            return static_cast<T>(value);
        } else if constexpr (detail::IsVector<T>::value) {
            RequireKind(Kind::kSequence, "sequence");
            T values; values.reserve(Size());
            for (std::size_t i = 0; i < Size(); ++i) values.push_back(At(i).template As<typename detail::IsVector<T>::Element>());
            return values;
        } else static_assert(!sizeof(T), "unsupported configuration type");
    }
    template <class T> T Get(std::string_view key) const { return At(key).template As<T>(); }
    template <class T> T GetOr(std::string_view key, T fallback) const {
        const auto value = Find(key);
        return value ? value->template As<T>() : std::move(fallback);
    }
    template <class T> T AsInRange(T minimum, T maximum) const {
        static_assert(std::is_arithmetic_v<T>, "range requires an arithmetic type");
        if (!(minimum <= maximum)) throw std::invalid_argument("invalid configuration schema range");
        const T value = As<T>();
        Require(value >= minimum && value <= maximum, "value outside allowed range");
        return value;
    }
 private:
    Node(std::shared_ptr<const detail::Value> value, std::string source, std::string path);
    std::shared_ptr<const detail::Value> value_;
    std::string source_, path_;
    void RequireKind(Kind kind, const char* expected) const;
    const std::string& Scalar() const;
    bool Boolean() const;
    std::int64_t Signed() const;
    std::uint64_t Unsigned() const;
    double Number() const;
    friend Node ParseYaml(std::string_view, const std::string&, LoadOptions);
};
Node ParseYaml(std::string_view text, const std::string& source = "<memory>", LoadOptions options = {});
Node LoadYamlFile(const std::string& path, LoadOptions options = {});
}  // namespace openember::config
