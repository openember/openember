/*
 * Copyright (c) 2022-2023, OpenEmber Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2022-07-07     luhuadong    the first version
 */

#ifndef __AG_CONFIG_H__
#define __AG_CONFIG_H__


#define AG_NAME_MAX         16
#define AG_USING_LIBC

/*
 * Choose internal message queue backend.
 * Current default: NNG (pub/sub) transport.
 */
//#define AG_LIBS_USING_MQTT
//#define AG_LIBS_USING_MQTT_CLIENT
//#define AG_LIBS_USING_MQTT_ASYNC
//#define AG_LIBS_USING_MQTT_MOSQUITTO
//#define AG_LIBS_USING_ZEROMQ

#define AG_LIBS_USING_NNG

/* Log definitions */
#define AG_LIBS_USING_LOG

#define AG_LIBS_USING_ZLOG
#ifdef AG_LIBS_USING_ZLOG
#define LOG_FILE    "/etc/openember/zlog.conf"
#endif

//#define AG_LIBS_USING_EASYLOGGER

#endif /* __AG_CONFIG_H__ */