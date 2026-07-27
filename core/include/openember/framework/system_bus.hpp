/*
 * Copyright (c) 2026, OpenEmber Team
 * SPDX-License-Identifier: Apache-2.0
 *
 * Helpers for framework nodes on OpenEmber Link (replaces msg_bus_* / msg_smm_register).
 */

#pragma once

#include <memory>
#include <string>

#include "ember_def.h"

#include "openember/node.hpp"

namespace openember::framework {

inline constexpr const char* kDefaultRobotId = "openember";

/** launch_manager: Zenoh router (listen tcp/127.0.0.1:7447). */
void InitSystemRouter(const std::string& robot_id = kDefaultRobotId);

/** Other system/services: Zenoh client connecting to the local router. */
void InitSystemClient(const std::string& robot_id = kDefaultRobotId);

/**
 * Publish module registration (smm_msg_t) on MOD_REGISTER_TOPIC.
 * Returns false if publish fails.
 */
bool RegisterModule(Node& node, const std::string& name, mod_class_t cls);

}  // namespace openember::framework
