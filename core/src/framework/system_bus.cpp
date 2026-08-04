/*
 * Copyright (c) 2026, OpenEmber Team
 * SPDX-License-Identifier: Apache-2.0
 */

#include "openember/framework/system_bus.hpp"

#include "openember/init.hpp"
#include "openember/link/options.hpp"

namespace openember::framework {

void InitSystemRouter(const std::string& robot_id)
{
    RuntimeOptions options;
    options.robot_id = robot_id;
    options.link = link::LocalRouterOptions();
    Init(options);
}

void InitSystemClient(const std::string& robot_id)
{
    RuntimeOptions options;
    options.robot_id = robot_id;
    options.link = link::LocalClientOptions();
    Init(options);
}

}  // namespace openember::framework
