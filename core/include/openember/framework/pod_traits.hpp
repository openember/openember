/*
 * Copyright (c) 2026, OpenEmber Team
 * SPDX-License-Identifier: Apache-2.0
 *
 * MessageTraits for legacy framework POD types (topic.h).
 */

#pragma once

#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>

#include "topic.h"

#include "openember/link/message_traits.hpp"

namespace openember::link {
namespace detail {

template <typename PodT>
transport::ByteBuffer SerializePod(const PodT& value)
{
    const auto* bytes = reinterpret_cast<const std::uint8_t*>(&value);
    return transport::ByteBuffer(bytes, bytes + sizeof(PodT));
}

template <typename PodT>
PodT DeserializePod(const transport::ByteBuffer& buffer, const char* type_name)
{
    if (buffer.size() < sizeof(PodT)) {
        throw std::runtime_error(std::string("POD payload too small for ") + type_name);
    }
    PodT value{};
    std::memcpy(&value, buffer.data(), sizeof(PodT));
    return value;
}

}  // namespace detail

template <>
struct MessageTraits<smm_msg_t> {
    static std::string TypeName() { return "openember/smm_msg"; }

    static transport::ByteBuffer Serialize(const smm_msg_t& value)
    {
        return detail::SerializePod(value);
    }

    static smm_msg_t Deserialize(const transport::ByteBuffer& buffer)
    {
        return detail::DeserializePod<smm_msg_t>(buffer, "smm_msg_t");
    }
};

template <>
struct MessageTraits<state_msg_t> {
    static std::string TypeName() { return "openember/state_msg"; }

    static transport::ByteBuffer Serialize(const state_msg_t& value)
    {
        return detail::SerializePod(value);
    }

    static state_msg_t Deserialize(const transport::ByteBuffer& buffer)
    {
        return detail::DeserializePod<state_msg_t>(buffer, "state_msg_t");
    }
};

template <>
struct MessageTraits<event_msg_t> {
    static std::string TypeName() { return "openember/event_msg"; }

    static transport::ByteBuffer Serialize(const event_msg_t& value)
    {
        return detail::SerializePod(value);
    }

    static event_msg_t Deserialize(const transport::ByteBuffer& buffer)
    {
        return detail::DeserializePod<event_msg_t>(buffer, "event_msg_t");
    }
};

template <>
struct MessageTraits<keepalive_msg_t> {
    static std::string TypeName() { return "openember/keepalive_msg"; }

    static transport::ByteBuffer Serialize(const keepalive_msg_t& value)
    {
        return detail::SerializePod(value);
    }

    static keepalive_msg_t Deserialize(const transport::ByteBuffer& buffer)
    {
        return detail::DeserializePod<keepalive_msg_t>(buffer, "keepalive_msg_t");
    }
};

}  // namespace openember::link
