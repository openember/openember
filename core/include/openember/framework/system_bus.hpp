/*
 * Copyright (c) 2026, OpenEmber Team
 * SPDX-License-Identifier: Apache-2.0
 *
 * Helpers for framework nodes on OpenEmber Link.
 */

#pragma once

#include <memory>
#include <string>

namespace openember::framework {

inline constexpr const char* kDefaultRobotId = "openember";

/** launch_manager: Zenoh router (listen tcp/127.0.0.1:7447). */
void InitSystemRouter(const std::string& robot_id = kDefaultRobotId);

/** Other system/services: Zenoh client connecting to the local router. */
void InitSystemClient(const std::string& robot_id = kDefaultRobotId);

}  // namespace openember::framework
