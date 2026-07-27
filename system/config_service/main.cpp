/*
 * Copyright (c) 2022-2026, OpenEmber Team
 * SPDX-License-Identifier: Apache-2.0
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <chrono>
#include <thread>

#define MODULE_NAME "config_service"
#define LOG_TAG MODULE_NAME
#include "openember.h"
#include "sqlite3.h"

#include "openember/framework/system_bus.hpp"
#include "openember/init.hpp"
#include "openember/node.hpp"

sqlite3* db;
sqlite3_stmt* stmt = 0;

static void Create(int rc, sqlite3* db, char* sql, sqlite3_stmt* stmt)
{
    rc = sqlite3_prepare_v2(db, sql, strlen(sql), &stmt, 0);
    if (rc) {
        LOG_E("对象转换失败:%s", sqlite3_errmsg(db));
        return;
    }
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

static void Insert(int rc, char* sql, sqlite3* db, sqlite3_stmt* stmt, char name[])
{
    sprintf(sql, "INSERT INTO MyTable VALUES(NULL,'%s');", name);
    rc = sqlite3_prepare_v2(db, sql, strlen(sql), &stmt, 0);
    if (rc) {
        LOG_E("对象转换失败:%s", sqlite3_errmsg(db));
        return;
    }
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

int database_init()
{
    char sql[] = "CREATE TABLE IF NOT EXISTS MyTable(ID integer NOT NULL primary "
                 "key autoincrement,Name nvarchar(32));";

    int rc = sqlite3_open("MyDB.db", &db);
    if (rc) {
        sqlite3_close(db);
        return -1;
    }
    Create(rc, db, sql, stmt);

    Insert(rc, sql, db, stmt, (char*)"张三");
    Insert(rc, sql, db, stmt, (char*)"李四");
    Insert(rc, sql, db, stmt, (char*)"王五");
    Insert(rc, sql, db, stmt, (char*)"赵六");

    sprintf(sql, "SELECT * FROM MyTable;");
    sqlite3_prepare_v2(db, sql, strlen(sql), &stmt, 0);
    int id;
    unsigned char* name;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        id = sqlite3_column_int(stmt, 0);
        name = (unsigned char*)sqlite3_column_text(stmt, 1);
        LOG_I("id: %d, name: %s", id, name);
    }
    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return 0;
}

int main(void)
{
    log_init(MODULE_NAME);
    LOG_I("Version: %lu.%lu.%lu", EMBER_VERSION, EMBER_SUBVERSION, EMBER_REVISION);

    try {
        openember::framework::InitSystemClient();
        auto node = openember::CreateNode(MODULE_NAME);
        if (!openember::framework::RegisterModule(*node, MODULE_NAME, SUBMODULE_CLASS_CONFIG)) {
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

    database_init();
    return 0;
}
