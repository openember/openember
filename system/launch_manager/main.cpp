/*
 * Copyright (c) 2022-2026, OpenEmber Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2022-07-07     luhuadong    the first version
 * 2026-07-25     openember    migrate from msgbus to Link
 */

#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>

#define APPLICATION_NAME "launch_manager"
#define LOG_TAG APPLICATION_NAME
#include "openember.h"

#include "openember/framework/pod_traits.hpp"
#include "openember/framework/system_bus.hpp"
#include "openember/init.hpp"
#include "openember/node.hpp"

#define EMBER_GLOBALS
#include "fsm.h"
#include "smm.h"

EMBER_EXT State context;

#define DEFAULT_FILE "/var/run/openember.pid"

static pid_t s_pid;
static int lock_fd;

static void sigroutine(int signal)
{
    switch (signal) {
    case 1:
        LOG_I("Get a signal -- SIGHUP");
        break;
    case 2:
        LOG_I("Get a signal -- SIGINT");
        smm_kill_all_modules();
        LOG_I("Exit!");
        openember::Shutdown();
        exit(0);
        break;
    case 3:
        LOG_I("Get a signal -- SIGQUIT");
        smm_stop_all_modules();
        LOG_I("Exit!");
        openember::Shutdown();
        exit(0);
        break;
    }
}

static int create_lock_file(const char* lockfile)
{
    lock_fd = open(lockfile, O_WRONLY | O_CREAT, 0666);
    if (lock_fd < 0) {
        perror("Fail to open");
        return -EMBER_ENOFILE;
    }

    struct flock lock;
    bzero(&lock, sizeof(lock));

    if (fcntl(lock_fd, F_GETLK, &lock) < 0) {
        perror("Fail to fcntl F_GETLK");
        close(lock_fd);
        LOG_I("%s has been running", APPLICATION_NAME);
    }

    lock.l_type = F_WRLCK;
    lock.l_whence = SEEK_SET;

    if (fcntl(lock_fd, F_SETLK, &lock) < 0) {
        perror("Fail to fcntl F_SETLK");
        close(lock_fd);
        LOG_I("%s has been running", APPLICATION_NAME);
        return -EMBER_ENOPERM;
    }

    char buf[32] = {0};
    s_pid = getpid();
    int len = snprintf(buf, 32, "%d\n", (int)s_pid);
    write(lock_fd, buf, len);
    return EMBER_EOK;
}

static void destroy_lock_file(int fd)
{
    if (fd) {
        close(fd);
    }
}

static int get_instance_pid(const char* lockfile)
{
    lock_fd = open(lockfile, O_RDONLY, 0644);
    if (lock_fd < 0) {
        perror("Fail to open");
        return -EMBER_ENOFILE;
    }

    char buf[32] = {0};
    int ret = read(lock_fd, buf, sizeof(buf));
    if (ret == -1) {
        LOG_E("read Error");
        close(lock_fd);
        return -EMBER_EIO;
    }

    LOG_I("PID: %s", buf);
    close(lock_fd);
    return atoi(buf);
}

static int startup_modules(void)
{
    system("/opt/openember/bin/health_monitor &");
    system("/opt/openember/bin/web_console &");
    system("/opt/openember/bin/ota_agent &");
    system("/opt/openember/bin/config_service &");
    system("/opt/openember/bin/device_manager &");
    system("/opt/openember/bin/sensor_data_reference &");
    return EMBER_EOK;
}

int main(int argc, char* argv[])
{
    int rc;

    if (argc > 1) {
        if (0 == strncmp("stop", argv[1], strlen("stop"))) {
            LOG_I("** Stop %s", APPLICATION_NAME);

            rc = create_lock_file(DEFAULT_FILE);
            if (rc == EMBER_EOK) {
                LOG_E("Lock file was not locked up");
                destroy_lock_file(lock_fd);
                exit(1);
            }

            s_pid = get_instance_pid(DEFAULT_FILE);
            LOG_D("Send SIGQUIT signal to %d", s_pid);
            kill(s_pid, SIGQUIT);
            LOG_D("Exit!");
            exit(1);
        }
    }

    log_init(APPLICATION_NAME);
    LOG_I("Start openember app, version: %lu.%lu.%lu", EMBER_VERSION, EMBER_SUBVERSION,
          EMBER_REVISION);
    LOG_I("Process id is %d", getpid());

    signal(SIGHUP, sigroutine);
    signal(SIGINT, sigroutine);
    signal(SIGQUIT, sigroutine);

    rc = create_lock_file(DEFAULT_FILE);
    if (rc != EMBER_EOK) {
        LOG_E("Can not create lock file");
        exit(1);
    }

    fsm_init();
    smm_init();

    try {
        openember::framework::InitSystemRouter();
    } catch (const std::exception& e) {
        LOG_E("Link init failed: %s", e.what());
        exit(1);
    }

    auto node = openember::CreateNode(APPLICATION_NAME);

    auto event_sub = node->Subscribe<event_msg_t>(
        SYS_EVENT_TOPIC, [](const event_msg_t& e) {
            LOG_I("event: [%d] %s", e.event_id, e.event_data.event_str);
            if (EMBER_EVENT_EXCEPTION == e.event_id) {
                context.goWrong();
            } else if (EMBER_EVENT_RECOVERY == e.event_id) {
                context.recovery();
            }
        });

    auto reg_sub = node->Subscribe<smm_msg_t>(
        MOD_REGISTER_TOPIC, [](const smm_msg_t& msg) {
            LOG_I("Register: %s, %d", msg.name, msg.pid);
            if (NULL == smm_register(msg.name, msg.cls, msg.pid, NULL)) {
                LOG_E("Module %s register failed.", msg.name);
            }
        });

    context.init();
    startup_modules();

    context.sleep();
    context.init();
    context.wakeUp();
    context.init();

    auto state_pub = node->Advertise<state_msg_t>(SYS_STATE_TOPIC);

    while (1) {
        state_msg_t stateMsg{};
        stateMsg.state_id = fsm_get_current_state();
        char* stateText = fsm_get_state_text(stateMsg.state_id);
        if (stateText) {
            strncpy(stateMsg.state_str, stateText, sizeof(stateMsg.state_str) - 1);
        }
        (void)state_pub.Publish(stateMsg);
        fsm_sem_wait();
    }

    openember::Shutdown();
    fsm_deinit();
    return 0;
}
