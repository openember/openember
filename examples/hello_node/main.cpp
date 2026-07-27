/*
 * Copyright (c) 2022-2026, OpenEmber Team
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2022-07-07     luhuadong    the first version
 * 2026-07-25     openember    migrate from msgbus to Link
 */

#include <unistd.h>

#include <chrono>
#include <memory>
#include <string>
#include <thread>

#include <nlohmann/json.hpp>

#define MODULE_NAME "hello_node"
#define LOG_TAG MODULE_NAME
#include "openember.h"

#include "openember/framework/system_bus.hpp"
#include "openember/init.hpp"
#include "openember/node.hpp"
#include "openember/thread_pool/thread_pool.hpp"

int main(void)
{
    log_init(MODULE_NAME);

    LOG_D("Hello OpenEmber!");
    LOG_I("Hello OpenEmber!");
    LOG_W("Hello OpenEmber!");
    LOG_E("Hello OpenEmber!");
    LOG_I("Version: %lu.%lu.%lu", EMBER_VERSION, EMBER_SUBVERSION, EMBER_REVISION);

    auto pool = std::make_unique<openember::thread_pool::ThreadPool>(5);

    try {
        openember::framework::InitSystemClient();
        auto node = openember::CreateNode(MODULE_NAME);

        auto sub = node->Subscribe<std::string>(
            TEST_TOPIC, [pool = pool.get()](const std::string& body) {
                if (!pool) {
                    return;
                }
                pool->post([body]() {
                    try {
                        nlohmann::json j = nlohmann::json::parse(body);
                        LOG_I("%s", j.dump(4).c_str());
                    } catch (const std::exception& e) {
                        LOG_E("JSON: %s", e.what());
                    }
                });
            });

        if (!openember::framework::RegisterModule(*node, MODULE_NAME, SUBmod_class_tEST)) {
            LOG_E("Module register publish failed.");
        }

        auto pub = node->Advertise<std::string>(TEST_TOPIC);
        int count = 3;
        while (count--) {
            char buf[256] = {0};
            snprintf(buf, sizeof(buf), "{\"id\":\"%d\",\"msg\":\"Hello, OpenEmber\"}", count);
            (void)pub.Publish(std::string(buf));
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }

        LOG_I("[Module] Template end.");
        pool->wait_idle();
        pool.reset();
        openember::Shutdown();
    } catch (const std::exception& e) {
        LOG_E("Link init failed: %s", e.what());
        log_deinit();
        return 1;
    }

    log_deinit();
    return 0;
}
