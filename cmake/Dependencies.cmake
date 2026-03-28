# 第三方依赖管理（FetchContent / Vendor / System）
#
# 目标：
# 1. 固定版本：避免上游漂移导致的不可预期行为
# 2. 可升级：升级只需要修改版本变量并重新配置构建
# 3. 可回退：当系统已有依赖时优先使用系统；当模式为 VENDOR 使用 download/_extracted/ 等本地源码目录

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

# SQLite amalgamation（与历史 third_party/sqlite 对齐的版本）
set(OPENEMBER_SQLITE_VERSION "3.39.2")
set(OPENEMBER_SQLITE_AMALGAMATION_NUM "3390200")
set(OPENEMBER_SQLITE_URL
    "https://www.sqlite.org/2022/sqlite-amalgamation-${OPENEMBER_SQLITE_AMALGAMATION_NUM}.zip")

include(${CMAKE_SOURCE_DIR}/cmake/GetSqlite.cmake)

# nlohmann/json、yaml-cpp、spdlog、Asio（版本唯一来源）
set(OPENEMBER_NLOHMANN_JSON_VERSION "3.11.3" CACHE STRING "nlohmann/json version")
set(OPENEMBER_NLOHMANN_JSON_URL
    "https://github.com/nlohmann/json/archive/refs/tags/v${OPENEMBER_NLOHMANN_JSON_VERSION}.tar.gz"
    CACHE STRING "nlohmann/json archive URL")

set(OPENEMBER_YAMLCPP_VERSION "0.8.0" CACHE STRING "yaml-cpp version")
set(OPENEMBER_YAMLCPP_URL
    "https://github.com/jbeder/yaml-cpp/archive/${OPENEMBER_YAMLCPP_VERSION}.tar.gz"
    CACHE STRING "yaml-cpp archive URL")

set(OPENEMBER_SPDLOG_VERSION "1.14.1" CACHE STRING "spdlog version")
set(OPENEMBER_SPDLOG_URL
    "https://github.com/gabime/spdlog/archive/refs/tags/v${OPENEMBER_SPDLOG_VERSION}.tar.gz"
    CACHE STRING "spdlog archive URL")

set(OPENEMBER_ASIO_TAG "asio-1-30-2" CACHE STRING "Asio Git tag (e.g. asio-1-30-2)")
set(OPENEMBER_ASIO_URL
    "https://github.com/chriskohlhoff/asio/archive/${OPENEMBER_ASIO_TAG}.tar.gz"
    CACHE STRING "Asio archive URL")

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

# Eclipse Paho MQTT C（components/mqtt/mqtt_client.cpp 直接使用 MQTTClient.h）。
set(OPENEMBER_PAHO_MQTT_C_VERSION "1.3.16")
set(OPENEMBER_PAHO_MQTT_C_URL
    "https://github.com/eclipse-paho/paho.mqtt.c/archive/v${OPENEMBER_PAHO_MQTT_C_VERSION}.tar.gz")

set(PAHO_MQTT_C_LOCAL_SOURCE "" CACHE PATH
    "Optional: pre-extracted paho.mqtt.c tree (overrides third_party/ archives and network download)")

# Mongoose embedded web server (used by apps/services/web_dashboard)
set(OPENEMBER_MONGOOSE_VERSION "7.20")
set(OPENEMBER_MONGOOSE_URL
    "https://github.com/cesanta/mongoose/archive/refs/tags/${OPENEMBER_MONGOOSE_VERSION}.tar.gz")

# ruckig（可选；运动轨迹生成，C++20）https://github.com/pantor/ruckig
set(OPENEMBER_RUCKIG_VERSION "0.17.3" CACHE STRING "ruckig release tag")
set(OPENEMBER_RUCKIG_URL
    "https://github.com/pantor/ruckig/archive/refs/tags/v${OPENEMBER_RUCKIG_VERSION}.tar.gz"
    CACHE STRING "ruckig source archive URL")

# 解压目录与 third_party 缓存文件名（与上游归档顶层目录一致）
set(OPENEMBER_ZLOG_CACHE_KEY "zlog-${OPENEMBER_ZLOG_VERSION}")
set(OPENEMBER_ZLOG_STAGE_DIR_NAME "${OPENEMBER_ZLOG_CACHE_KEY}")
set(OPENEMBER_CJSON_CACHE_KEY "cJSON-${OPENEMBER_CJSON_VERSION}")
set(OPENEMBER_CJSON_STAGE_DIR_NAME "${OPENEMBER_CJSON_CACHE_KEY}")
set(OPENEMBER_NLOHMANN_JSON_CACHE_KEY "json-${OPENEMBER_NLOHMANN_JSON_VERSION}")
set(OPENEMBER_NLOHMANN_JSON_STAGE_DIR_NAME "${OPENEMBER_NLOHMANN_JSON_CACHE_KEY}")
set(OPENEMBER_SQLITE_CACHE_KEY "sqlite-amalgamation-${OPENEMBER_SQLITE_AMALGAMATION_NUM}")
set(OPENEMBER_SQLITE_STAGE_DIR_NAME "${OPENEMBER_SQLITE_CACHE_KEY}")
set(OPENEMBER_MONGOOSE_CACHE_KEY "mongoose-${OPENEMBER_MONGOOSE_VERSION}")
set(OPENEMBER_MONGOOSE_STAGE_DIR_NAME "${OPENEMBER_MONGOOSE_CACHE_KEY}")
set(OPENEMBER_YAMLCPP_CACHE_KEY "yaml-cpp-${OPENEMBER_YAMLCPP_VERSION}")
set(OPENEMBER_YAMLCPP_STAGE_DIR_NAME "${OPENEMBER_YAMLCPP_CACHE_KEY}")
set(OPENEMBER_SPDLOG_CACHE_KEY "spdlog-${OPENEMBER_SPDLOG_VERSION}")
set(OPENEMBER_SPDLOG_STAGE_DIR_NAME "${OPENEMBER_SPDLOG_CACHE_KEY}")
set(OPENEMBER_ASIO_CACHE_KEY "asio-${OPENEMBER_ASIO_TAG}")
set(OPENEMBER_ASIO_STAGE_DIR_NAME "${OPENEMBER_ASIO_CACHE_KEY}")
set(OPENEMBER_LIBZMQ_CACHE_KEY "libzmq-${OPENEMBER_LIBZMQ_VERSION}")
set(OPENEMBER_LIBZMQ_STAGE_DIR_NAME "${OPENEMBER_LIBZMQ_CACHE_KEY}")
set(OPENEMBER_NNG_CACHE_KEY "nng-${OPENEMBER_NNG_VERSION}")
set(OPENEMBER_NNG_STAGE_DIR_NAME "${OPENEMBER_NNG_CACHE_KEY}")
set(OPENEMBER_LCM_CACHE_KEY "lcm-${OPENEMBER_LCM_VERSION}")
set(OPENEMBER_LCM_STAGE_DIR_NAME "${OPENEMBER_LCM_CACHE_KEY}")
set(OPENEMBER_CPPZMQ_CACHE_KEY "cppzmq-${OPENEMBER_CPPZMQ_VERSION}")
set(OPENEMBER_CPPZMQ_STAGE_DIR_NAME "${OPENEMBER_CPPZMQ_CACHE_KEY}")
set(OPENEMBER_PAHO_MQTT_C_CACHE_KEY "paho.mqtt.c-${OPENEMBER_PAHO_MQTT_C_VERSION}")
set(OPENEMBER_PAHO_MQTT_C_STAGE_DIR_NAME "${OPENEMBER_PAHO_MQTT_C_CACHE_KEY}")
set(OPENEMBER_RUCKIG_CACHE_KEY "ruckig-${OPENEMBER_RUCKIG_VERSION}")
set(OPENEMBER_RUCKIG_STAGE_DIR_NAME "${OPENEMBER_RUCKIG_CACHE_KEY}")

set(OPENEMBER_ZLOG_LOCAL_SOURCE "" CACHE PATH "Optional: pre-extracted zlog tree")
set(OPENEMBER_CJSON_LOCAL_SOURCE "" CACHE PATH "Optional: pre-extracted cJSON tree")
set(OPENEMBER_NLOHMANN_JSON_LOCAL_SOURCE "" CACHE PATH "Optional: pre-extracted nlohmann/json tree")
set(OPENEMBER_SQLITE_LOCAL_SOURCE "" CACHE PATH "Optional: SQLite amalgamation dir (sqlite3.c)")
set(OPENEMBER_MONGOOSE_LOCAL_SOURCE "" CACHE PATH "Optional: pre-extracted mongoose tree")
set(OPENEMBER_YAMLCPP_LOCAL_SOURCE "" CACHE PATH "Optional: pre-extracted yaml-cpp tree")
set(OPENEMBER_ASIO_LOCAL_SOURCE "" CACHE PATH "Optional: pre-extracted Asio tree")
set(OPENEMBER_SPDLOG_LOCAL_SOURCE "" CACHE PATH "Optional: pre-extracted spdlog tree")
set(OPENEMBER_LIBZMQ_LOCAL_SOURCE "" CACHE PATH "Optional: pre-extracted libzmq tree")
set(OPENEMBER_NNG_LOCAL_SOURCE "" CACHE PATH "Optional: pre-extracted nng tree")
set(OPENEMBER_LCM_LOCAL_SOURCE "" CACHE PATH "Optional: pre-extracted lcm tree")
set(OPENEMBER_CPPZMQ_LOCAL_SOURCE "" CACHE PATH "Optional: pre-extracted cppzmq tree")
set(OPENEMBER_RUCKIG_LOCAL_SOURCE "" CACHE PATH "Optional: pre-extracted ruckig tree")

include(${CMAKE_SOURCE_DIR}/cmake/GetNlohmannJson.cmake)
include(${CMAKE_SOURCE_DIR}/cmake/GetYamlCpp.cmake)
include(${CMAKE_SOURCE_DIR}/cmake/GetAsio.cmake)
include(${CMAKE_SOURCE_DIR}/cmake/GetSpdlog.cmake)
include(${CMAKE_SOURCE_DIR}/cmake/GetMongoose.cmake)
include(${CMAKE_SOURCE_DIR}/cmake/GetRuckig.cmake)

function(openember_third_party_resolve_zlog)
    # 仅当 OPENEMBER_LOG_BACKEND=ZLOG 时解析 zlog（由根 CMakeLists 在 include 本文件前设置）
    if(NOT OPENEMBER_LOG_BACKEND STREQUAL "ZLOG")
        set(ZLOG_INCLUDE_DIRS "" PARENT_SCOPE)
        set(ZLOG_LIBRARIES "" PARENT_SCOPE)
        set(BUILD_ZLOG FALSE PARENT_SCOPE)
        return()
    endif()

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

    if(OPENEMBER_THIRD_PARTY_MODE STREQUAL "FETCH" OR OPENEMBER_THIRD_PARTY_MODE STREQUAL "VENDOR")
        if(NOT OPENEMBER_THIRD_PARTY_BUNDLE_ZLOG)
            message(FATAL_ERROR
                "OPENEMBER_THIRD_PARTY_BUNDLE_ZLOG=OFF but ZLOG backend needs zlog sources. "
                "Install zlog system-wide, or enable the bundle in menuconfig (Third party).")
        endif()
    endif()

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

    if(OPENEMBER_THIRD_PARTY_MODE STREQUAL "FETCH" OR OPENEMBER_THIRD_PARTY_MODE STREQUAL "VENDOR")
        if(NOT OPENEMBER_THIRD_PARTY_BUNDLE_CJSON)
            message(FATAL_ERROR
                "OPENEMBER_THIRD_PARTY_BUNDLE_CJSON=OFF but JSON library is cJSON. "
                "Install libcjson system-wide or enable bundle (Third party).")
        endif()
    endif()

    set(ENABLE_CJSON_TEST OFF CACHE BOOL "Disable cJSON test build" FORCE)
    openember_get_cjson()
    set(BUILD_CJSON FALSE PARENT_SCOPE)
    set(CJSON_INCLUDE_DIRS ${CJSON_INCLUDE_DIRS} PARENT_SCOPE)
    set(CJSON_LIBRARIES ${CJSON_LIBRARIES} PARENT_SCOPE)
endfunction()

function(openember_third_party_resolve_nlohmann_json)
    if(OPENEMBER_THIRD_PARTY_MODE STREQUAL "SYSTEM")
        find_package(nlohmann_json CONFIG REQUIRED)
        return()
    endif()

    find_package(nlohmann_json CONFIG QUIET)
    if(nlohmann_json_FOUND)
        return()
    endif()

    if(OPENEMBER_THIRD_PARTY_MODE STREQUAL "FETCH" OR OPENEMBER_THIRD_PARTY_MODE STREQUAL "VENDOR")
        if(NOT OPENEMBER_THIRD_PARTY_BUNDLE_NLOHMANN_JSON)
            message(FATAL_ERROR
                "OPENEMBER_THIRD_PARTY_BUNDLE_NLOHMANN_JSON=OFF but JSON library is nlohmann/json. "
                "Install nlohmann-json3-dev package or enable bundle (Third party).")
        endif()
    endif()

    openember_get_nlohmann_json()
endfunction()

function(openember_third_party_resolve_paho_mqtt_c)
    # Provide legacy interface:
    # - PAHO_MQTT_C_INCLUDE_DIRS
    # - PAHO_MQTT_C_LIBRARIES

    # SYSTEM: prefer find_package（PAHO_WITH_SSL 与 FindPahoMqttC 一致）
    if(OPENEMBER_THIRD_PARTY_MODE STREQUAL "SYSTEM")
        if(DEFINED OPENEMBER_MQTT_PAHO_TLS AND NOT OPENEMBER_MQTT_PAHO_TLS)
            set(PAHO_WITH_SSL OFF)
        else()
            set(PAHO_WITH_SSL ON)
        endif()
        find_package(PahoMqttC QUIET)
        if(NOT PahoMqttC_FOUND)
            message(FATAL_ERROR "PahoMqttC not found but OPENEMBER_THIRD_PARTY_MODE=SYSTEM")
        endif()
        if(NOT OPENEMBER_MQTT_PAHO_ASYNC)
            set(PAHO_MQTT_C_LIBRARIES ${PAHO_MQTT_C_3C_LIBRARY})
        endif()
        set(BUILD_MQTT FALSE PARENT_SCOPE)
        set(PAHO_MQTT_C_INCLUDE_DIRS ${PAHO_MQTT_C_INCLUDE_DIRS} PARENT_SCOPE)
        set(PAHO_MQTT_C_LIBRARIES ${PAHO_MQTT_C_LIBRARIES} PARENT_SCOPE)
        return()
    endif()

    if(OPENEMBER_THIRD_PARTY_MODE STREQUAL "FETCH" OR OPENEMBER_THIRD_PARTY_MODE STREQUAL "VENDOR")
        if(NOT OPENEMBER_THIRD_PARTY_BUNDLE_PAHO)
            message(FATAL_ERROR
                "OPENEMBER_THIRD_PARTY_BUNDLE_PAHO=OFF but MQTT component is enabled. "
                "Install Paho MQTT C system-wide or enable bundle (Third party).")
        endif()
    endif()

    openember_get_paho_mqtt_c()
    set(BUILD_MQTT FALSE PARENT_SCOPE)
    set(PAHO_MQTT_C_INCLUDE_DIRS ${PAHO_MQTT_C_INCLUDE_DIRS} PARENT_SCOPE)
    set(PAHO_MQTT_C_LIBRARIES ${PAHO_MQTT_C_LIBRARIES} PARENT_SCOPE)
endfunction()

function(openember_third_party_resolve_sqlite)
    if(OPENEMBER_THIRD_PARTY_MODE STREQUAL "SYSTEM")
        find_path(_sqlite_inc NAMES sqlite3.h)
        find_library(_sqlite_lib NAMES sqlite3)
        if(NOT _sqlite_inc OR NOT _sqlite_lib)
            message(FATAL_ERROR "sqlite3 / sqlite3.h not found but OPENEMBER_THIRD_PARTY_MODE=SYSTEM")
        endif()
        set(SQLITE_INCLUDE_DIRS ${_sqlite_inc} PARENT_SCOPE)
        set(SQLITE_LIBRARIES ${_sqlite_lib} PARENT_SCOPE)
        return()
    endif()

    if(OPENEMBER_THIRD_PARTY_MODE STREQUAL "FETCH" OR OPENEMBER_THIRD_PARTY_MODE STREQUAL "VENDOR")
        if(NOT OPENEMBER_THIRD_PARTY_BUNDLE_SQLITE)
            message(FATAL_ERROR
                "OPENEMBER_THIRD_PARTY_BUNDLE_SQLITE=OFF: install sqlite3 dev package or enable bundle (Third party).")
        endif()
    endif()

    openember_get_sqlite()
    set(SQLITE_INCLUDE_DIRS ${SQLITE_INCLUDE_DIRS} PARENT_SCOPE)
    set(SQLITE_LIBRARIES ${SQLITE_LIBRARIES} PARENT_SCOPE)
endfunction()

function(openember_third_party_resolve_mongoose)
    # SYSTEM: require system-installed mongoose lib/header
    if(OPENEMBER_THIRD_PARTY_MODE STREQUAL "SYSTEM")
        find_path(MONGOOSE_INCLUDE_DIRS NAMES mongoose.h)
        find_library(_mongoose_lib NAMES mongoose)
        if(NOT MONGOOSE_INCLUDE_DIRS OR NOT _mongoose_lib)
            message(FATAL_ERROR "mongoose not found but OPENEMBER_THIRD_PARTY_MODE=SYSTEM")
        endif()
        set(MONGOOSE_INCLUDE_DIRS ${MONGOOSE_INCLUDE_DIRS} PARENT_SCOPE)
        set(MONGOOSE_LIBRARIES ${_mongoose_lib} PARENT_SCOPE)
        return()
    endif()

    if(OPENEMBER_THIRD_PARTY_MODE STREQUAL "FETCH" OR OPENEMBER_THIRD_PARTY_MODE STREQUAL "VENDOR")
        if(NOT OPENEMBER_THIRD_PARTY_BUNDLE_MONGOOSE)
            message(FATAL_ERROR
                "OPENEMBER_THIRD_PARTY_BUNDLE_MONGOOSE=OFF: install mongoose dev package or enable bundle (Third party).")
        endif()
    endif()

    openember_get_mongoose()
    set(MONGOOSE_INCLUDE_DIRS ${MONGOOSE_INCLUDE_DIRS} PARENT_SCOPE)
    set(MONGOOSE_LIBRARIES ${MONGOOSE_LIBRARIES} PARENT_SCOPE)
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

    if(OPENEMBER_THIRD_PARTY_MODE STREQUAL "FETCH" OR OPENEMBER_THIRD_PARTY_MODE STREQUAL "VENDOR")
        if(NOT OPENEMBER_THIRD_PARTY_BUNDLE_LIBZMQ)
            message(FATAL_ERROR
                "OPENEMBER_THIRD_PARTY_BUNDLE_LIBZMQ=OFF: install libzmq-dev or enable bundle (Third party).")
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

    if(OPENEMBER_THIRD_PARTY_MODE STREQUAL "FETCH" OR OPENEMBER_THIRD_PARTY_MODE STREQUAL "VENDOR")
        if(NOT OPENEMBER_THIRD_PARTY_BUNDLE_NNG)
            message(FATAL_ERROR
                "OPENEMBER_THIRD_PARTY_BUNDLE_NNG=OFF: install libnng-dev or enable bundle (Third party).")
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

    if(OPENEMBER_THIRD_PARTY_MODE STREQUAL "FETCH" OR OPENEMBER_THIRD_PARTY_MODE STREQUAL "VENDOR")
        if(NOT OPENEMBER_THIRD_PARTY_BUNDLE_LCM)
            message(FATAL_ERROR
                "OPENEMBER_THIRD_PARTY_BUNDLE_LCM=OFF: install liblcm-dev or enable bundle (Third party).")
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

    if(OPENEMBER_THIRD_PARTY_MODE STREQUAL "FETCH" OR OPENEMBER_THIRD_PARTY_MODE STREQUAL "VENDOR")
        if(NOT OPENEMBER_THIRD_PARTY_BUNDLE_CPPZMQ)
            message(FATAL_ERROR
                "OPENEMBER_THIRD_PARTY_BUNDLE_CPPZMQ=OFF: install cppzmq headers or enable bundle (Third party).")
        endif()
    endif()

    openember_get_cppzmq()
    set(OPENEMBER_CPPZMQ_INCLUDE_DIRS ${OPENEMBER_CPPZMQ_INCLUDE_DIRS} PARENT_SCOPE)
endfunction()

# 可选 C++ 依赖：yaml-cpp / Asio；spdlog 仅在 OPENEMBER_LOG_BACKEND=SPDLOG 时拉取
function(openember_third_party_resolve_optional_cxx_deps)
    if(OPENEMBER_WITH_YAMLCPP)
        if(OPENEMBER_THIRD_PARTY_MODE STREQUAL "SYSTEM")
            find_package(yaml-cpp REQUIRED)
        else()
            if(OPENEMBER_THIRD_PARTY_MODE STREQUAL "FETCH" OR OPENEMBER_THIRD_PARTY_MODE STREQUAL "VENDOR")
                if(NOT OPENEMBER_THIRD_PARTY_BUNDLE_YAMLCPP)
                    message(FATAL_ERROR
                        "OPENEMBER_THIRD_PARTY_BUNDLE_YAMLCPP=OFF: install libyaml-cpp-dev or enable bundle (Third party).")
                endif()
            endif()
            openember_get_yaml_cpp()
        endif()
    endif()

    if(OPENEMBER_WITH_ASIO)
        if(OPENEMBER_THIRD_PARTY_MODE STREQUAL "SYSTEM")
            find_path(OPENEMBER_ASIO_INCLUDE_DIR "asio.hpp" PATH_SUFFIXES "asio/include")
            if(NOT OPENEMBER_ASIO_INCLUDE_DIR)
                message(FATAL_ERROR "SYSTEM mode: standalone Asio headers not found (asio.hpp)")
            endif()
            if(NOT TARGET openember::asio)
                add_library(openember_asio_sys INTERFACE)
                add_library(openember::asio ALIAS openember_asio_sys)
                target_include_directories(openember_asio_sys INTERFACE ${OPENEMBER_ASIO_INCLUDE_DIR})
                target_compile_definitions(openember_asio_sys INTERFACE ASIO_STANDALONE ASIO_NO_DEPRECATED)
                find_package(Threads REQUIRED)
                target_link_libraries(openember_asio_sys INTERFACE Threads::Threads)
            endif()
        else()
            if(OPENEMBER_THIRD_PARTY_MODE STREQUAL "FETCH" OR OPENEMBER_THIRD_PARTY_MODE STREQUAL "VENDOR")
                if(NOT OPENEMBER_THIRD_PARTY_BUNDLE_ASIO)
                    message(FATAL_ERROR
                        "OPENEMBER_THIRD_PARTY_BUNDLE_ASIO=OFF: install Asio headers or enable bundle (Third party).")
                endif()
            endif()
            openember_get_asio()
        endif()
    endif()

    if(OPENEMBER_LOG_BACKEND STREQUAL "SPDLOG")
        if(OPENEMBER_THIRD_PARTY_MODE STREQUAL "SYSTEM")
            find_package(spdlog REQUIRED)
        else()
            if(OPENEMBER_THIRD_PARTY_MODE STREQUAL "FETCH" OR OPENEMBER_THIRD_PARTY_MODE STREQUAL "VENDOR")
                if(NOT OPENEMBER_THIRD_PARTY_BUNDLE_SPDLOG)
                    message(FATAL_ERROR
                        "OPENEMBER_THIRD_PARTY_BUNDLE_SPDLOG=OFF: install libspdlog-dev or enable bundle (Third party).")
                endif()
            endif()
            openember_get_spdlog()
        endif()
    endif()

    if(OPENEMBER_WITH_RUCKIG)
        if(OPENEMBER_THIRD_PARTY_MODE STREQUAL "SYSTEM")
            message(FATAL_ERROR
                "OPENEMBER_WITH_RUCKIG=ON: SYSTEM 模式未配置 ruckig 探测，请改用 FETCH/VENDOR 或关闭该选项。")
        endif()
        if(OPENEMBER_THIRD_PARTY_MODE STREQUAL "FETCH" OR OPENEMBER_THIRD_PARTY_MODE STREQUAL "VENDOR")
            if(NOT OPENEMBER_RUCKIG_LOCAL_SOURCE AND NOT OPENEMBER_THIRD_PARTY_BUNDLE_RUCKIG)
                message(FATAL_ERROR
                    "OPENEMBER_THIRD_PARTY_BUNDLE_RUCKIG=OFF but OPENEMBER_WITH_RUCKIG=ON. "
                    "Enable bundle (Third party) or set OPENEMBER_RUCKIG_LOCAL_SOURCE.")
            endif()
        endif()
        openember_get_ruckig()
    endif()
endfunction()


