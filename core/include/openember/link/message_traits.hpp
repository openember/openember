#pragma once

#include <cstddef>
#include <limits>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <typeinfo>
#include <utility>

#include "openember/transport/buffer.hpp"

namespace openember::link {
namespace detail {

template <typename>
struct AlwaysFalse : std::false_type {};

template <typename T, typename = void>
struct IsProtobufMessage : std::false_type {};

template <typename T>
struct IsProtobufMessage<T, std::void_t<
    decltype(std::declval<const T&>().SerializeToString(std::declval<std::string*>())),
    decltype(std::declval<T&>().ParseFromArray(std::declval<const void*>(), int{})),
    decltype(T::descriptor())>> : std::true_type {};

}  // namespace detail

template <typename T>
inline constexpr bool IsProtobufMessageV = detail::IsProtobufMessage<T>::value;

template <typename T, typename Enable = void>
struct MessageTraits {
    static std::string TypeName() {
        static_assert(detail::AlwaysFalse<T>::value,
                      "MessageTraits<T> is not implemented for this type");
        return {};
    }

    static transport::ByteBuffer Serialize(const T&) {
        static_assert(detail::AlwaysFalse<T>::value,
                      "MessageTraits<T> is not implemented for this type");
        return {};
    }

    static T Deserialize(const transport::ByteBuffer&) {
        static_assert(detail::AlwaysFalse<T>::value,
                      "MessageTraits<T> is not implemented for this type");
        return {};
    }
};

template <>
struct MessageTraits<std::string> {
    static std::string TypeName() {
        return "std_msgs/String";
    }

    static transport::ByteBuffer Serialize(const std::string& value) {
        return transport::ByteBuffer(value.begin(), value.end());
    }

    static std::string Deserialize(const transport::ByteBuffer& buffer) {
        return std::string(buffer.begin(), buffer.end());
    }
};

template <>
struct MessageTraits<transport::ByteBuffer> {
    static std::string TypeName() {
        return "openember/ByteBuffer";
    }

    static transport::ByteBuffer Serialize(const transport::ByteBuffer& value) {
        return value;
    }

    static transport::ByteBuffer Deserialize(const transport::ByteBuffer& buffer) {
        return buffer;
    }
};

template <typename T>
struct MessageTraits<T, std::enable_if_t<IsProtobufMessageV<T>>> {
    static std::string TypeName() {
        const auto* descriptor = T::descriptor();
        if (descriptor == nullptr) {
            return typeid(T).name();
        }
        return descriptor->full_name();
    }

    static transport::ByteBuffer Serialize(const T& value) {
        std::string bytes;
        if (!value.SerializeToString(&bytes)) {
            throw std::runtime_error("failed to serialize protobuf message: " + TypeName());
        }
        return transport::ByteBuffer(bytes.begin(), bytes.end());
    }

    static T Deserialize(const transport::ByteBuffer& buffer) {
        T value;
        if (buffer.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
            throw std::runtime_error("protobuf payload is too large: " + TypeName());
        }
        if (!value.ParseFromArray(buffer.data(), static_cast<int>(buffer.size()))) {
            throw std::runtime_error("failed to parse protobuf message: " + TypeName());
        }
        return value;
    }
};

}  // namespace openember::link
