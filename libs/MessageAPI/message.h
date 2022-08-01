/*
 * Copyright (c) 2006-2020, DreamGrow Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2022-07-07     luhuadong    the first version
 */

#ifndef __AG_MSG_WRAPPER_H__
#define __AG_MSG_WRAPPER_H__

#include "agloo.h"

#ifdef AG_LIBS_USING_MQTT
#include "mqtt_wrapper.h"
#endif

#ifdef AG_LIBS_USING_ZEROMQ
#include "zmq_wrapper.h"
#endif

#endif /* __AG_MSG_WRAPPER_H__ */