/*
 * Copyright (c) 2022-2026, OpenEmber Team
 * SPDX-License-Identifier: Apache-2.0
 */

#include <cstdlib>
#include <chrono>
#include <thread>

#define MODULE_NAME "sensor_data_reference"
#define LOG_TAG MODULE_NAME
#include "openember.h"

#include "openember/framework/system_bus.hpp"
#include "openember/init.hpp"
#include "openember/node.hpp"

int main(void)
{
    log_init(MODULE_NAME);
    LOG_I("Version: %lu.%lu.%lu", EMBER_VERSION, EMBER_SUBVERSION, EMBER_REVISION);

    try {
        openember::framework::InitSystemClient();
        auto node = openember::CreateNode(MODULE_NAME);
        if (!openember::framework::RegisterModule(*node, MODULE_NAME,
                                                  SUBMODULE_CLASS_ACQUISITION)) {
            LOG_E("Module register publish failed.");
            openember::Shutdown();
            return 1;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        openember::Shutdown();
    } catch (const std::exception& e) {
        LOG_E("Link init failed: %s", e.what());
        return 1;
    }
    return 0;
}
