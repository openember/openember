#pragma once

#include <stdexcept>
#include <string>

#include "openember/link/message_traits.hpp"
#include "openember/transport/buffer.hpp"

namespace openember {

template <typename T>
transport::ByteBuffer Serialize(const T& value) {
    return link::MessageTraits<T>::Serialize(value);
}

template <typename T>
T Deserialize(const transport::ByteBuffer& buffer) {
    return link::MessageTraits<T>::Deserialize(buffer);
}

}  // namespace openember
