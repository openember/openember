#pragma once

#include <stdexcept>
#include <string>

#include "openember/transport/buffer.hpp"

namespace openember {

template <typename T>
transport::ByteBuffer Serialize(const T& value) {
    static_assert(sizeof(T) == 0,
                  "Serialize<T> is not implemented for this type");
    (void)value;
    return {};
}

template <typename T>
T Deserialize(const transport::ByteBuffer& buffer) {
    static_assert(sizeof(T) == 0,
                  "Deserialize<T> is not implemented for this type");
    (void)buffer;
    return {};
}

template <>
inline transport::ByteBuffer Serialize<std::string>(const std::string& value) {
    return transport::ByteBuffer(value.begin(), value.end());
}

template <>
inline std::string Deserialize<std::string>(
    const transport::ByteBuffer& buffer) {
    return std::string(buffer.begin(), buffer.end());
}

template <>
inline transport::ByteBuffer Serialize<transport::ByteBuffer>(
    const transport::ByteBuffer& value) {
    return value;
}

template <>
inline transport::ByteBuffer Deserialize<transport::ByteBuffer>(
    const transport::ByteBuffer& buffer) {
    return buffer;
}

}  // namespace openember