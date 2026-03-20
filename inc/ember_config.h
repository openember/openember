/*
 * Copyright (c) 2022-2023, OpenEmber Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2022-07-07     luhuadong    the first version
 */

#ifndef __EMBER_CONFIG_H__
#define __EMBER_CONFIG_H__


#define EMBER_NAME_MAX         16
#define EMBER_USING_LIBC

/*
 * Choose internal message queue backend.
 * Current default: NNG (pub/sub) transport.
 */
//#define EMBER_LIBS_USING_MQTT
//#define EMBER_LIBS_USING_MQTT_CLIENT
//#define EMBER_LIBS_USING_MQTT_ASYNC
//#define EMBER_LIBS_USING_MQTT_MOSQUITTO
//#define EMBER_LIBS_USING_ZEROMQ

/*
 * Internal msgbus backend selection.
 *
 * By default (no backend macro is provided), we use NNG.
 * When building with LCM, CMake should define EMBER_LIBS_USING_LCM
 * and the default NNG macro will not be enabled here.
 */
#if !defined(EMBER_LIBS_USING_MQTT) && !defined(EMBER_LIBS_USING_ZEROMQ) && \
    !defined(EMBER_LIBS_USING_NNG) && !defined(EMBER_LIBS_USING_LCM)
#define EMBER_LIBS_USING_NNG
#endif

/* Log definitions */
#define EMBER_LIBS_USING_LOG

#define EMBER_LIBS_USING_ZLOG
#ifdef EMBER_LIBS_USING_ZLOG
#define LOG_FILE    "/etc/openember/zlog.conf"
#endif

//#define EMBER_LIBS_USING_EASYLOGGER

#endif /* __EMBER_CONFIG_H__ */