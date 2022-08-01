/*
 * Copyright (c) 2006-2020, DreamGrow Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2022-07-07     luhuadong    the first version
 */

#include <stdio.h>
#include <stdlib.h>
#include "agloo.h"

int main(int argc, char *argv[])
{
    sayHello("Workflow Controller");

    system("/opt/agloo/bin/MessageDispatcher &");
    system("/opt/agloo/bin/MonitorAlarm &");
    system("/opt/agloo/bin/WebServer &");
    system("/opt/agloo/bin/OTA &");
    system("/opt/agloo/bin/ConfigManager &");
    system("/opt/agloo/bin/DeviceManager &");
    system("/opt/agloo/bin/Acquisition &");

    while (1)
    {
        sleep(1);
    }
    
    return 0;
}