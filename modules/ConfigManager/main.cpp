/*
 * Copyright (c) 2022-2023, OpenEmber Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2022-07-07     luhuadong    the first version
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>

#define MODULE_NAME            "ConfigMng"
#define LOG_TAG                MODULE_NAME
#include "agloo.h"
#include "sqlite3.h"

sqlite3 *db;
sqlite3_stmt *stmt=0;

static msg_node_t client;

static void _msg_arrived_cb(char *topic, void *payload, size_t payloadlen)
{
    LOG_D("[%s] %s\n", topic, (char *)payload);
}

static int msg_init(void)
{
    int rc = 0, cn = 0;

    rc = msg_bus_init(&client, MODULE_NAME, NULL, _msg_arrived_cb);
    if (rc != AG_EOK) {
        printf("Message bus init failed.\n");
        return -1;
    }

    /* Subscription list */
    rc = msg_bus_subscribe(client, TEST_TOPIC);
    if (rc != AG_EOK) cn++;
    rc = msg_bus_subscribe(client, SYS_EVENT_REPLY_TOPIC);
    if (rc != AG_EOK) cn++;
    rc = msg_bus_subscribe(client, MOD_REGISTER_REPLY_TOPIC);
    if (rc != AG_EOK) cn++;

    if (cn != 0) {
        msg_bus_deinit(client);
        printf("Message bus subscribe failed.\n");
        return -AG_ERROR;
    }

    return AG_EOK;
}


static void Create(int rc,sqlite3 *db,char *sql,sqlite3_stmt *stmt)
{
	//判断是否已经存在表了
	rc=sqlite3_prepare_v2(db,sql,strlen(sql),&stmt,0);
	if (rc)
	{
		fprintf(stderr,"对象转换失败:%s\n",sqlite3_errmsg(db));
		return;
	}
	//执行stmt(执行SQL语句)
	sqlite3_step(stmt);
	//释放stmt资源
	sqlite3_finalize(stmt);
}
 
static void Insert(int rc,char *sql,sqlite3 *db,sqlite3_stmt *stmt,char name[])
{
	sprintf(sql,"INSERT INTO MyTable VALUES(NULL,'%s');",name);
	rc=sqlite3_prepare_v2(db,sql,strlen(sql),&stmt,0);
	if (rc)
	{
		fprintf(stderr,"对象转换失败:%s\n",sqlite3_errmsg(db));
		return;
	}
	sqlite3_step(stmt);
	sqlite3_finalize(stmt);
}

int database_init()
{
    char sql[]=	"CREATE TABLE IF NOT EXISTS MyTable(ID integer NOT NULL primary "
		"key autoincrement,Name nvarchar(32));";
    
    int rc=sqlite3_open("MyDB.db",&db);
	if (rc)
	{
		sqlite3_close(db);
		return -1;
	}
	else
		Create(rc,db,sql,stmt);
	
	//向数据里边插入些数据,用来测试
	Insert(rc,sql,db,stmt,"张三");
	Insert(rc,sql,db,stmt,"李四");
	Insert(rc,sql,db,stmt,"王五");
	Insert(rc,sql,db,stmt,"赵六");
	
	sprintf(sql,"SELECT * FROM MyTable;");
	//[2]将SQL语句转换成stmt对象
	sqlite3_prepare_v2(db,sql,strlen(sql),&stmt,0);
	int id;
	unsigned char *name;
	//[3]循环执行stmt对象,读取数据库里边的数据
	while(sqlite3_step(stmt)==SQLITE_ROW)
	{
		//[4]绑定变量
		id=sqlite3_column_int(stmt,0);
		name=(unsigned char *)sqlite3_column_text(stmt,1);
		LOG_I("id: %d, name: %s", id, name);
	}
	//[5]释放资源
	sqlite3_finalize(stmt);
	//[6]关闭数据库
	sqlite3_close(db);
	return 0;
}

int main(void)
{
    int rc;

    sayHello(MODULE_NAME);
    
    log_init(MODULE_NAME);
    LOG_I("Version: %lu.%lu.%lu", AG_VERSION, AG_SUBVERSION, AG_REVISION);

    rc = msg_init();
    if (rc != AG_EOK) {
        LOG_E("Message channel init failed.");
        exit(1);
    }

    rc = msg_smm_register(client, MODULE_NAME, SUBMODULE_CLASS_CONFIG);
    if (rc != AG_EOK) {
        LOG_E("Module register failed.");
        exit(1);
    }

    database_init();

    return 0;
}