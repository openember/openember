/*
 * Copyright (c) 2026, OpenEmber Team
 * SPDX-License-Identifier: Apache-2.0
 *
 * Type support for OpenEmber.
 * 
 * TODO:
 * 第一版先用这个满足 introspection 的基础需求，后面再换成注册式类型系统。
 */

#pragma once

#include <string>
#include <typeinfo>

#include "openember/link/message_traits.hpp"
#include "openember/transport/buffer.hpp"

namespace openember {

template <typename T>
inline std::string TypeName() {
    return link::MessageTraits<T>::TypeName();
}

}  // namespace openember
