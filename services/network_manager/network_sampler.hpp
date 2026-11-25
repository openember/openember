// Copyright (c) 2022-2026, OpenEmber Team
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "openember/msgs/network/v1/network.pb.h"
#include <cstddef>
#include <string>

namespace openember::network_manager {
using Snapshot = openember::msgs::network::v1::NetworkStateSnapshot;

// Unprivileged, bounded RTM_GETLINK/RTM_GETADDR dumps. Never changes interfaces.
Snapshot Sample();

namespace detail {
// Decode one RTM_NEWLINK/RTM_NEWADDR payload; shared by collection and wire tests.
bool DecodeLink(const void* payload, std::size_t size, Snapshot& result);
bool DecodeAddress(const void* payload, std::size_t size, Snapshot& result);
}  // namespace detail
}  // namespace openember::network_manager
