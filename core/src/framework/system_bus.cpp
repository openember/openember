/*
 * Copyright (c) 2026, OpenEmber Team
 * SPDX-License-Identifier: Apache-2.0
 */

#include "openember/framework/system_bus.hpp"

#include <cstring>
#include <unistd.h>

#include "topic.h"

#include "openember/framework/pod_traits.hpp"
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

bool RegisterModule(Node& node, const std::string& name, mod_class_t cls)
{
    smm_msg_t msg{};
    std::strncpy(msg.name, name.c_str(), sizeof(msg.name) - 1);
    msg.cls = cls;
    msg.pid = getpid();

    auto pub = node.Advertise<smm_msg_t>(MOD_REGISTER_TOPIC);
    return pub.Publish(msg);
}

}  // namespace openember::framework
