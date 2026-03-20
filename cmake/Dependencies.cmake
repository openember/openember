# 第三方依赖管理（FetchContent / Vendor / System）
#
# 目标：
# 1. 固定版本：避免上游漂移导致的不可预期行为
# 2. 可升级：升级只需要修改版本变量并重新配置构建
# 3. 可回退：当系统已有依赖时优先使用系统；当模式为 VENDOR 使用仓库内 third_party

set(OPENEMBER_THIRD_PARTY_MODE "FETCH" CACHE STRING
    "Third-party source mode: FETCH/VENDOR/SYSTEM")
set_property(CACHE OPENEMBER_THIRD_PARTY_MODE PROPERTY STRINGS FETCH VENDOR SYSTEM)

# -----------------------------
# Version pins (可按需升级)
# -----------------------------
set(OPENEMBER_ZLOG_VERSION "1.2.16")
set(OPENEMBER_ZLOG_URL
    "https://github.com/HardySimpson/zlog/archive/refs/tags/${OPENEMBER_ZLOG_VERSION}.tar.gz")

set(OPENEMBER_CJSON_VERSION "1.7.15")
set(OPENEMBER_CJSON_URL
    "https://repository.timesys.com/buildsources/c/cJSON/cJSON-${OPENEMBER_CJSON_VERSION}/cJSON-${OPENEMBER_CJSON_VERSION}.tar.gz")

include(${CMAKE_SOURCE_DIR}/cmake/GetZlog.cmake)
include(${CMAKE_SOURCE_DIR}/cmake/GetCjson.cmake)
include(${CMAKE_SOURCE_DIR}/cmake/GetPahoMqttC.cmake)

# ------------------------------------------------------------
# Transport dependencies (nng / lcm / libzmq / cppzmq)
# ------------------------------------------------------------
set(OPENEMBER_LIBZMQ_VERSION "4.3.5")
set(OPENEMBER_LIBZMQ_URL
    "https://github.com/zeromq/libzmq/archive/refs/tags/v${OPENEMBER_LIBZMQ_VERSION}.tar.gz")

set(OPENEMBER_NNG_VERSION "2.0.0-alpha.7")
set(OPENEMBER_NNG_URL
    "https://github.com/nanomsg/nng/archive/refs/tags/v${OPENEMBER_NNG_VERSION}.tar.gz")

set(OPENEMBER_LCM_VERSION "1.5.2")
set(OPENEMBER_LCM_URL
    "https://github.com/lcm-proj/lcm/archive/refs/tags/v${OPENEMBER_LCM_VERSION}.tar.gz")

set(OPENEMBER_CPPZMQ_VERSION "4.11.0")
set(OPENEMBER_CPPZMQ_URL
    "https://github.com/zeromq/cppzmq/archive/refs/tags/v${OPENEMBER_CPPZMQ_VERSION}.tar.gz")

include(${CMAKE_SOURCE_DIR}/cmake/GetLibzmq.cmake)
include(${CMAKE_SOURCE_DIR}/cmake/GetNng.cmake)
include(${CMAKE_SOURCE_DIR}/cmake/GetLcm.cmake)
include(${CMAKE_SOURCE_DIR}/cmake/GetCppZmq.cmake)

# paho.mqtt.c 固定版本（可按需升级）
set(OPENEMBER_PAHO_MQTT_C_VERSION "1.3.13")
set(OPENEMBER_PAHO_MQTT_C_URL
    "https://github.com/eclipse-paho/paho.mqtt.c/archive/v${OPENEMBER_PAHO_MQTT_C_VERSION}.tar.gz")

function(openember_third_party_resolve_zlog)
    # Prefer system package if available
    find_package(ZLOG QUIET)
    if(OPENEMBER_THIRD_PARTY_MODE STREQUAL "SYSTEM")
        if(NOT ZLOG_FOUND)
            message(FATAL_ERROR "ZLOG not found but OPENEMBER_THIRD_PARTY_MODE=SYSTEM")
        endif()
        set(BUILD_ZLOG FALSE PARENT_SCOPE)
        set(ZLOG_INCLUDE_DIRS ${ZLOG_INCLUDE_DIRS} PARENT_SCOPE)
        set(ZLOG_LIBRARIES ${ZLOG_LIBRARIES} PARENT_SCOPE)
        return()
    endif()

    if(ZLOG_FOUND)
        set(BUILD_ZLOG FALSE PARENT_SCOPE)
        set(ZLOG_INCLUDE_DIRS ${ZLOG_INCLUDE_DIRS} PARENT_SCOPE)
        set(ZLOG_LIBRARIES ${ZLOG_LIBRARIES} PARENT_SCOPE)
        return()
    endif()

    if(OPENEMBER_THIRD_PARTY_MODE STREQUAL "VENDOR")
        set(BUILD_ZLOG TRUE PARENT_SCOPE)
        set(ZLOG_INCLUDE_DIRS ${CMAKE_SOURCE_DIR}/third_party/zlog/src PARENT_SCOPE)
        set(ZLOG_LIBRARIES zlog PARENT_SCOPE)
        return()
    endif()

    # FETCH
    openember_get_zlog()
    set(BUILD_ZLOG FALSE PARENT_SCOPE)
    set(ZLOG_INCLUDE_DIRS ${ZLOG_INCLUDE_DIRS} PARENT_SCOPE)
    set(ZLOG_LIBRARIES ${ZLOG_LIBRARIES} PARENT_SCOPE)
endfunction()

function(openember_third_party_resolve_cjson)
    # Prefer system package if available
    find_package(cJSON QUIET)
    if(OPENEMBER_THIRD_PARTY_MODE STREQUAL "SYSTEM")
        if(NOT cJSON_FOUND)
            message(FATAL_ERROR "cJSON not found but OPENEMBER_THIRD_PARTY_MODE=SYSTEM")
        endif()
        set(BUILD_CJSON FALSE PARENT_SCOPE)
        set(CJSON_INCLUDE_DIRS ${CJSON_INCLUDE_DIRS} PARENT_SCOPE)
        set(CJSON_LIBRARIES ${CJSON_LIBRARIES} PARENT_SCOPE)
        return()
    endif()

    if(cJSON_FOUND)
        set(BUILD_CJSON FALSE PARENT_SCOPE)
        set(CJSON_INCLUDE_DIRS ${CJSON_INCLUDE_DIRS} PARENT_SCOPE)
        set(CJSON_LIBRARIES ${CJSON_LIBRARIES} PARENT_SCOPE)
        return()
    endif()

    if(OPENEMBER_THIRD_PARTY_MODE STREQUAL "VENDOR")
        # cJSON 的 upstream 默认会构建 Unity 测试框架（target 名叫 unity），
        # 在同一工程中可能与其它依赖的测试目标发生冲突。
        # 对 OpenEmber 构建链路而言，这些测试不需要，因此强制关闭。
        set(ENABLE_CJSON_TEST OFF CACHE BOOL "Disable cJSON test build" FORCE)
        set(BUILD_CJSON TRUE PARENT_SCOPE)
        set(CJSON_INCLUDE_DIRS ${CMAKE_SOURCE_DIR}/third_party/cJSON PARENT_SCOPE)
        set(CJSON_LIBRARIES cjson PARENT_SCOPE)
        return()
    endif()

    # FETCH
    openember_get_cjson()
    set(BUILD_CJSON FALSE PARENT_SCOPE)
    set(CJSON_INCLUDE_DIRS ${CJSON_INCLUDE_DIRS} PARENT_SCOPE)
    set(CJSON_LIBRARIES ${CJSON_LIBRARIES} PARENT_SCOPE)
endfunction()

function(openember_third_party_resolve_paho_mqtt_c)
    # Provide legacy interface:
    # - PAHO_MQTT_C_INCLUDE_DIRS
    # - PAHO_MQTT_C_LIBRARIES

    # SYSTEM: prefer find_package
    if(OPENEMBER_THIRD_PARTY_MODE STREQUAL "SYSTEM")
        find_package(PahoMqttC QUIET)
        if(NOT PahoMqttC_FOUND)
            message(FATAL_ERROR "PahoMqttC not found but OPENEMBER_THIRD_PARTY_MODE=SYSTEM")
        endif()
        set(BUILD_MQTT FALSE PARENT_SCOPE)
        set(PAHO_MQTT_C_INCLUDE_DIRS ${PAHO_MQTT_C_INCLUDE_DIRS} PARENT_SCOPE)
        set(PAHO_MQTT_C_LIBRARIES ${PAHO_MQTT_C_LIBRARIES} PARENT_SCOPE)
        return()
    endif()

    # VENDOR: use bundled sources and let third_party/CMakeLists build them.
    if(OPENEMBER_THIRD_PARTY_MODE STREQUAL "VENDOR")
        set(BUILD_MQTT TRUE PARENT_SCOPE)
        set(PAHO_MQTT_C_INCLUDE_DIRS ${CMAKE_SOURCE_DIR}/third_party/paho.mqtt.c/src PARENT_SCOPE)
        set(PAHO_MQTT_C_LIBRARIES paho-mqtt3a paho-mqtt3c PARENT_SCOPE)
        return()
    endif()

    # FETCH: download + build via GetPahoMqttC.cmake
    openember_get_paho_mqtt_c()
    set(BUILD_MQTT FALSE PARENT_SCOPE) # Avoid building vendor paho again
    # Values are exported from openember_get_paho_mqtt_c() to this scope via PARENT_SCOPE,
    # then re-export to the top-level via PARENT_SCOPE.
    set(PAHO_MQTT_C_INCLUDE_DIRS ${PAHO_MQTT_C_INCLUDE_DIRS} PARENT_SCOPE)
    set(PAHO_MQTT_C_LIBRARIES ${PAHO_MQTT_C_LIBRARIES} PARENT_SCOPE)
endfunction()

################################################################################
# Transport resolve_* (FetchContent / Vendor / System)
################################################################################

function(openember_transport_resolve_libzmq)
    # SYSTEM: 依赖系统安装；FETCH/VENDOR: 走 FetchContent（VENDOR 依赖 *_LOCAL_SOURCE）
    if(OPENEMBER_THIRD_PARTY_MODE STREQUAL "SYSTEM")
        find_path(OPENEMBER_LIBZMQ_INCLUDE_DIRS NAMES zmq.h)
        find_library(_libzmq_lib NAMES zmq)
        if(NOT OPENEMBER_LIBZMQ_INCLUDE_DIRS OR NOT _libzmq_lib)
            message(FATAL_ERROR "libzmq not found but OPENEMBER_THIRD_PARTY_MODE=SYSTEM")
        endif()
        set(OPENEMBER_LIBZMQ_LIBRARIES ${_libzmq_lib} PARENT_SCOPE)
        return()
    endif()

    if(OPENEMBER_THIRD_PARTY_MODE STREQUAL "VENDOR")
        if(NOT OPENEMBER_LIBZMQ_LOCAL_SOURCE)
            set(OPENEMBER_LIBZMQ_LOCAL_SOURCE
                "${CMAKE_SOURCE_DIR}/download/_extracted/libzmq-${OPENEMBER_LIBZMQ_VERSION}"
                CACHE PATH "Local source override for libzmq (VENDOR mode)" FORCE)
        endif()
    endif()

    openember_get_libzmq()
    set(OPENEMBER_LIBZMQ_LIBRARIES ${OPENEMBER_LIBZMQ_LIBRARIES} PARENT_SCOPE)
    set(OPENEMBER_LIBZMQ_INCLUDE_DIRS ${OPENEMBER_LIBZMQ_INCLUDE_DIRS} PARENT_SCOPE)
endfunction()

function(openember_transport_resolve_nng)
    if(OPENEMBER_THIRD_PARTY_MODE STREQUAL "SYSTEM")
        find_path(OPENEMBER_NNG_INCLUDE_DIRS NAMES nng/nng.h)
        find_library(_nng_lib NAMES nng)
        if(NOT OPENEMBER_NNG_INCLUDE_DIRS OR NOT _nng_lib)
            message(FATAL_ERROR "nng not found but OPENEMBER_THIRD_PARTY_MODE=SYSTEM")
        endif()
        set(OPENEMBER_NNG_LIBRARIES ${_nng_lib} PARENT_SCOPE)
        return()
    endif()

    if(OPENEMBER_THIRD_PARTY_MODE STREQUAL "VENDOR")
        if(NOT OPENEMBER_NNG_LOCAL_SOURCE)
            set(OPENEMBER_NNG_LOCAL_SOURCE
                "${CMAKE_SOURCE_DIR}/download/_extracted/nng-${OPENEMBER_NNG_VERSION}"
                CACHE PATH "Local source override for nng (VENDOR mode)" FORCE)
        endif()
    endif()

    openember_get_nng()
    set(OPENEMBER_NNG_LIBRARIES ${OPENEMBER_NNG_LIBRARIES} PARENT_SCOPE)
    set(OPENEMBER_NNG_INCLUDE_DIRS ${OPENEMBER_NNG_INCLUDE_DIRS} PARENT_SCOPE)
endfunction()

function(openember_transport_resolve_lcm)
    if(OPENEMBER_THIRD_PARTY_MODE STREQUAL "SYSTEM")
        find_path(OPENEMBER_LCM_INCLUDE_DIRS NAMES lcm/lcm.h)
        find_library(_lcm_lib NAMES lcm-static lcm)
        if(NOT OPENEMBER_LCM_INCLUDE_DIRS OR NOT _lcm_lib)
            message(FATAL_ERROR "lcm not found but OPENEMBER_THIRD_PARTY_MODE=SYSTEM")
        endif()
        set(OPENEMBER_LCM_LIBRARIES ${_lcm_lib} PARENT_SCOPE)
        return()
    endif()

    if(OPENEMBER_THIRD_PARTY_MODE STREQUAL "VENDOR")
        if(NOT OPENEMBER_LCM_LOCAL_SOURCE)
            set(OPENEMBER_LCM_LOCAL_SOURCE
                "${CMAKE_SOURCE_DIR}/download/_extracted/lcm-${OPENEMBER_LCM_VERSION}"
                CACHE PATH "Local source override for lcm (VENDOR mode)" FORCE)
        endif()
    endif()

    openember_get_lcm()
    set(OPENEMBER_LCM_LIBRARIES ${OPENEMBER_LCM_LIBRARIES} PARENT_SCOPE)
    set(OPENEMBER_LCM_INCLUDE_DIRS ${OPENEMBER_LCM_INCLUDE_DIRS} PARENT_SCOPE)
endfunction()

function(openember_transport_resolve_cppzmq)
    # cppzmq is a header-only C++ binding.
    if(OPENEMBER_THIRD_PARTY_MODE STREQUAL "SYSTEM")
        find_path(OPENEMBER_CPPZMQ_INCLUDE_DIRS NAMES zmq.hpp)
        if(NOT OPENEMBER_CPPZMQ_INCLUDE_DIRS)
            message(FATAL_ERROR "cppzmq not found but OPENEMBER_THIRD_PARTY_MODE=SYSTEM")
        endif()
        return()
    endif()

    if(OPENEMBER_THIRD_PARTY_MODE STREQUAL "VENDOR")
        if(NOT OPENEMBER_CPPZMQ_LOCAL_SOURCE)
            set(OPENEMBER_CPPZMQ_LOCAL_SOURCE
                "${CMAKE_SOURCE_DIR}/download/_extracted/cppzmq-${OPENEMBER_CPPZMQ_VERSION}"
                CACHE PATH "Local source override for cppzmq (VENDOR mode)" FORCE)
        endif()
    endif()

    openember_get_cppzmq()
    set(OPENEMBER_CPPZMQ_INCLUDE_DIRS ${OPENEMBER_CPPZMQ_INCLUDE_DIRS} PARENT_SCOPE)
endfunction()


