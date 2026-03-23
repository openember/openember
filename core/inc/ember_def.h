/*
 * Copyright (c) 2022-2023, OpenEmber Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2022-07-07     luhuadong    the first version
 */

#ifndef __EMBER_DEF_H__
#define __EMBER_DEF_H__

/* include agconfig header to import configuration */
#include "ember_config.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef EMBER_GLOBALS
#define EMBER_EXT
#else
#define EMBER_EXT                          extern
#endif

/* Version information */
#define EMBER_VERSION                      1L              /* major version number */
#define EMBER_SUBVERSION                   0L              /* minor version number */
#define EMBER_REVISION                     0L              /* revise version number */

/* Lidar software version */
#define EMBER_LIDAR_VERSION                ((EMBER_VERSION * 10000) + \
                                         (EMBER_SUBVERSION * 100) + EMBER_REVISION)

/* Error code definitions */
#define EMBER_EOK                          0               /* There is no error */
#define EMBER_ERROR                        1               /* A generic error happens */
#define EMBER_ETIMEOUT                     2               /* Timed out */
#define EMBER_EFULL                        3               /* The resource is full */
#define EMBER_EEMPTY                       4               /* The resource is empty */
#define EMBER_ENOMEM                       5               /* No memory */
#define EMBER_ENOSYS                       6               /* No system */
#define EMBER_EBUSY                        7               /* Busy */
#define EMBER_EIO                          8               /* IO error */
#define EMBER_EINTR                        9               /* Interrupted system call */
#define EMBER_EINVAL                       10              /* Invalid argument */
#define EMBER_ENOFILE                      11              /* No file */
#define EMBER_ENOPERM                      12              /* NO permission */

/* Basic data type definitions */
#ifdef EMBER_USING_LIBC
#include <stdint.h>
#include <stddef.h>
typedef int8_t                          ember_int8_t;      /*  8bit integer type */
typedef int16_t                         ember_int16_t;     /* 16bit integer type */
typedef int32_t                         ember_int32_t;     /* 32bit integer type */
typedef uint8_t                         ember_uint8_t;     /*  8bit unsigned integer type */
typedef uint16_t                        ember_uint16_t;    /* 16bit unsigned integer type */
typedef uint32_t                        ember_uint32_t;    /* 32bit unsigned integer type */
typedef int64_t                         ember_int64_t;     /* 64bit integer type */
typedef uint64_t                        ember_uint64_t;    /* 64bit unsigned integer type */
typedef size_t                          ember_size_t;      /* Type for size number */

#else
typedef signed   char                   ember_int8_t;      /*  8bit integer type */
typedef signed   short                  ember_int16_t;     /* 16bit integer type */
typedef signed   int                    ember_int32_t;     /* 32bit integer type */
typedef unsigned char                   ember_uint8_t;     /*  8bit unsigned integer type */
typedef unsigned short                  ember_uint16_t;    /* 16bit unsigned integer type */
typedef unsigned int                    ember_uint32_t;    /* 32bit unsigned integer type */

#ifdef ARCH_CPU_64BIT
typedef signed long                     ember_int64_t;     /* 64bit integer type */
typedef unsigned long                   ember_uint64_t;    /* 64bit unsigned integer type */
typedef unsigned long                   ember_size_t;      /* Type for size number */
#else
typedef signed long long                ember_int64_t;     /* 64bit integer type */
typedef unsigned long long              ember_uint64_t;    /* 64bit unsigned integer type */
typedef unsigned int                    ember_size_t;      /* Type for size number */
#endif /* ARCH_CPU_64BIT */
#endif /* EMBER_USING_LIBC */

typedef int                             ember_bool_t;      /* boolean type */
typedef long                            ember_base_t;      /* Nbit CPU related date type */
typedef unsigned long                   ember_ubase_t;     /* Nbit unsigned CPU related data type */

typedef ember_base_t                       ember_err_t;       /* Type for error number */
typedef ember_uint32_t                     ember_time_t;      /* Type for time stamp */
typedef ember_base_t                       ember_flag_t;      /* Type for flags */
typedef ember_ubase_t                      ember_dev_t;       /* Type for device */
typedef ember_base_t                       ember_off_t;       /* Type for offset */

/* boolean type definitions */
#define EMBER_TRUE                         1               /* boolean true  */
#define EMBER_FALSE                        0               /* boolean fails */

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(arr)                 (sizeof(arr) / sizeof((arr)[0]))
#endif

/* System state definitions */
typedef enum state {
    EMBER_SYSTEM_POWEROFF = 0,  /* Power-off status */
    EMBER_SYSTEM_INIT,          /* Initialized status */
    EMBER_SYSTEM_NORMAL,        /* Normal status */
    EMBER_SYSTEM_ERROR,         /* Error status */
    EMBER_SYSTEM_DEGRADE,       /* Degrade status */
    EMBER_SYSTEM_SLEEP,         /* Sleep status */
    EMBER_SYSTEM_COUNT
} state_t;

/* Submodule state definitions */
typedef enum submod_state {
    EMBER_MODULE_INIT = 0,      /* Initialized status */
    EMBER_MODULE_READY,         /* Ready status */
    EMBER_MODULE_RUNNING,       /* Running or runnable (on run queue) */
    EMBER_MODULE_STOPPED,       /* Stop status */
    EMBER_MODULE_SLEEP,         /* Sleep status */
    EMBER_MODULE_DEAD,          /* Dead status */
    EMBER_MODULE_COUNT
} mod_state_t;

typedef enum submod_prio {
    SUBMODULE_PRIO_1 = 1,
    SUBMODULE_PRIO_2,
    SUBMODULE_PRIO_3,
    SUBMODULE_PRIO_4,
    SUBMODULE_PRIO_5,
    SUBMODULE_PRIO_6,
    SUBMODULE_PRIO_7,
    SUBMODULE_PRIO_8
} mod_prio_t;

typedef enum submod_class {
    SUBMODULE_CLASS_MESSAGE = 0,
    SUBMODULE_CLASS_DEVICE,
    SUBMODULE_CLASS_CONFIG,
    SUBMODULE_CLASS_MONITOR,
    SUBMODULE_CLASS_WEB,
    SUBMODULE_CLASS_OTA,
    SUBMODULE_CLASS_ACQUISITION,
    SUBmod_class_tEST,
    SUBMODULE_CLASS_MAX
} mod_class_t;

typedef enum event {
    EMBER_EVENT_NONE = 0,
    EMBER_EVENT_EXCEPTION,
    EMBER_EVENT_RECOVERY,
    EMBER_EVENT_REBOOT,
    EMBER_EVENT_POWEROFF,
    EMBER_EVENT_WAKEUP,
    EMBER_EVENT_COUNT
} event_t;

typedef enum control {
    EMBER_CONTROL_START = 0,
    EMBER_CONTROL_STOP,
    EMBER_CONTROL_PAUSE,
    EMBER_CONTROL_COUNT
} control_t;

#ifdef __cplusplus
}
#endif

#endif /* __EMBER_DEF_H__ */
