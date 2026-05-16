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

#include "openember/transport/buffer.hpp"

namespace openember {

template <typename T>
inline std::string TypeName() {
    return typeid(T).name();
}

template <>
inline std::string TypeName<std::string>() {
    return "std_msgs/String";
}

template <>
inline std::string TypeName<transport::ByteBuffer>() {
    return "openember/ByteBuffer";
}

}  // namespace openember