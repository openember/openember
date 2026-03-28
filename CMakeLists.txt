cmake_minimum_required(VERSION 3.1.0)

project(OpenEmber VERSION 1.0.0 LANGUAGES CXX C)

set(CMAKE_THREAD_LIBS_INIT "-lpthread")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -pthread")
set(CMAKE_HAVE_THREADS_LIBRARY 1)
set(CMAKE_USE_WIN32_THREADS_INIT 0)
set(CMAKE_USE_PTHREADS_INIT 1)
set(THREADS_PREFER_PTHREAD_FLAG ON)

################################################################################
# Include CMake dependencies
################################################################################
set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)

# Include helper macros and commands, and allow the included file to override
# the CMake policies in this file
include(${CMAKE_SOURCE_DIR}/cmake/OpenEmberCMakeHelper.cmake NO_POLICY_SCOPE)

# Third-party dependency policy (FetchContent / Vendor / System)
include(${CMAKE_SOURCE_DIR}/cmake/Dependencies.cmake)

# Kconfig generated cache/config (optional)
if(EXISTS "${CMAKE_BINARY_DIR}/config.cmake")
    include("${CMAKE_BINARY_DIR}/config.cmake")
endif()

################################################################################
# Options
################################################################################
option( DEBUG_ENABLED "Whether to enable debug mode" ON)
option( OPTIMIZATION_DISABLED "Whether to disable the optimization of compilation. " OFF)
option( CROSSCOMPILE_ENABLED "Whether to build for ARM" OFF)
option( OPENMP_ENABLED "Whether to enable omp feature" OFF)
option( TESTS_ENABLED "Whether to build/register unit tests (test/); prefer Kconfig test/Kconfig" OFF)
option( OPENEMBER_ENABLE_OSAL "Build platform OSAL (Linux pthread)" ON)
option( OPENEMBER_ENABLE_HAL "Build platform HAL (Linux file/uart; requires OSAL)" ON)
option( EXAMPLES_ENABLED "Whether compile examples" ON)
option( OPENEMBER_COMPONENT_MQTT "Build MQTT component (Paho C + openember_mqtt)" ON)
option( OPENEMBER_MQTT_PAHO_TLS "Build Paho with OpenSSL (TLS / ssl://)" ON)
option( OPENEMBER_MQTT_PAHO_ASYNC "Link Paho MQTTAsync in addition to MQTTClient" OFF)
option( OPENEMBER_EXAMPLE_MQTT_EMQX "Build example mqtt_emqx_client (Paho MQTT C → broker)" OFF)
option( BUILD_CJSON "Whether compile cJSON" OFF)
option( BUILD_MQTT "Whether compile MQTT" OFF)
option( BUILD_MD5 "Whether compile md5" OFF)
option( BUILD_ZLOG "Whether compile zlog" OFF)
option( BUILD_EASYLOGGER "Whether compile EasyLogger (obsolete, currently disabled)" OFF)

option( OPENEMBER_MSGBUS_USE_NNG "Use NNG backend for internal msgbus" ON)
option( OPENEMBER_MSGBUS_USE_LCM "Use LCM backend for internal msgbus" OFF)
# When LCM is selected: use C++ lcm::LCM for TransportBackend (MsgBusNode); C msg_bus_* still uses lcm_wrapper.c
option( OPENEMBER_MSGBUS_LCM_USE_CPP "Use C++ lcm::LCM for TransportBackend when LCM msgbus is enabled" ON)

if(OPENEMBER_MSGBUS_USE_NNG AND OPENEMBER_MSGBUS_USE_LCM)
    message(FATAL_ERROR "OPENEMBER_MSGBUS_USE_NNG and OPENEMBER_MSGBUS_USE_LCM cannot both be ON. Please select exactly one.")
endif()

if(OPENEMBER_MSGBUS_USE_LCM)
    set(_OPENEMBER_MSGBUS_HINT "lcm")
elseif(OPENEMBER_MSGBUS_USE_NNG)
    set(_OPENEMBER_MSGBUS_HINT "nng")
else()
    set(_OPENEMBER_MSGBUS_HINT "nng")
endif()
message(STATUS "msgbus: backend = ${_OPENEMBER_MSGBUS_HINT}")

if (DEBUG_ENABLED)
    set(CMAKE_BUILD_TYPE "Debug")
endif()

if (OPTIMIZATION_DISABLED)
    set(CMAKE_C_FLAGS "-Werror -O0 -Wno-deprecated-declarations")
    set(CMAKE_C_FLAGS_DEBUG "-g")
    set(CMAKE_C_FLAGS_RELEASE "-O0")
endif()

if (CROSSCOMPILE_ENABLED)
    set(CMAKE_SYSTEM_NAME Linux)
    set(CMAKE_SYSTEM_PROCESSOR aarch64)
    set(target_arch aarch64-linux-gnu)
    set(CMAKE_C_COMPILER /usr/bin/aarch64-linux-gnu-gcc)
    set(CMAKE_CXX_COMPILER /usr/bin/aarch64-linux-gnu-g++)
    set(CMAKE_LIBRARY_ARCHITECTURE ${target_arch} CACHE STRING "" FORCE)
    set(CMAKE_FIND_ROOT_PATH /opt/petalinux/2021.2/sysroots/cortexa72-cortexa53-xilinx-linux)
    # Search for programs in the build host directories
    #set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
    # Libraries and headers in the target directories
    #set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
    #set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
    #set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
endif()

if (OPENMP_ENABLED)
    add_definitions(-DOPENMP_ENABLED)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fopenmp")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fopenmp")
endif()

################################################################################
# Log backend (BUILTIN / ZLOG / SPDLOG) + generated ember_log_backend.h
################################################################################
if(NOT DEFINED OPENEMBER_LOG_BACKEND)
    set(OPENEMBER_LOG_BACKEND "ZLOG" CACHE STRING "OpenEmber logging backend")
endif()
set_property(CACHE OPENEMBER_LOG_BACKEND PROPERTY STRINGS BUILTIN ZLOG SPDLOG)

option(OPENEMBER_WITH_YAMLCPP "Fetch/use yaml-cpp (optional C++ dependency)" ON)
option(OPENEMBER_WITH_ASIO "Fetch/use standalone Asio (optional)" ON)

set(OPENEMBER_LOG_BACKEND_IS_BUILTIN 0)
set(OPENEMBER_LOG_BACKEND_IS_ZLOG 0)
set(OPENEMBER_LOG_BACKEND_IS_SPDLOG 0)
if(OPENEMBER_LOG_BACKEND STREQUAL "BUILTIN")
    set(OPENEMBER_LOG_BACKEND_IS_BUILTIN 1)
elseif(OPENEMBER_LOG_BACKEND STREQUAL "ZLOG")
    set(OPENEMBER_LOG_BACKEND_IS_ZLOG 1)
elseif(OPENEMBER_LOG_BACKEND STREQUAL "SPDLOG")
    set(OPENEMBER_LOG_BACKEND_IS_SPDLOG 1)
else()
    message(FATAL_ERROR "OPENEMBER_LOG_BACKEND must be BUILTIN, ZLOG, or SPDLOG (got '${OPENEMBER_LOG_BACKEND}')")
endif()

if(OPENEMBER_LOG_BACKEND STREQUAL "ZLOG")
    if(NOT DEFINED OPENEMBER_LOG_FILE OR "${OPENEMBER_LOG_FILE}" STREQUAL "")
        set(OPENEMBER_LOG_FILE "/etc/openember/zlog.conf" CACHE STRING "zlog.conf path" FORCE)
    endif()
endif()

configure_file(
    ${CMAKE_SOURCE_DIR}/core/inc/ember_log_backend.h.in
    ${CMAKE_BINARY_DIR}/include/ember_log_backend.h
    @ONLY)

configure_file(
    ${CMAKE_SOURCE_DIR}/core/inc/ember_log_path.h.in
    ${CMAKE_BINARY_DIR}/include/ember_log_path.h
    @ONLY)

################################################################################
# JSON 库选择：cJSON（C）或 nlohmann/json（C++）
################################################################################
if(NOT DEFINED OPENEMBER_JSON_LIBRARY)
    set(OPENEMBER_JSON_LIBRARY "CJSON" CACHE STRING "JSON implementation for OpenEmber")
endif()
set_property(CACHE OPENEMBER_JSON_LIBRARY PROPERTY STRINGS CJSON NLOHMANN_JSON)

set(OPENEMBER_JSON_USE_CJSON 0)
set(OPENEMBER_JSON_USE_NLOHMANN_JSON 0)
if(OPENEMBER_JSON_LIBRARY STREQUAL "CJSON")
    set(OPENEMBER_JSON_USE_CJSON 1)
elseif(OPENEMBER_JSON_LIBRARY STREQUAL "NLOHMANN_JSON")
    set(OPENEMBER_JSON_USE_NLOHMANN_JSON 1)
else()
    message(FATAL_ERROR "OPENEMBER_JSON_LIBRARY must be CJSON or NLOHMANN_JSON (got '${OPENEMBER_JSON_LIBRARY}')")
endif()

configure_file(
    ${CMAKE_SOURCE_DIR}/core/inc/ember_json_config.h.in
    ${CMAKE_BINARY_DIR}/include/ember_json_config.h
    @ONLY)

################################################################################
# Resolve third-party dependencies
################################################################################

# zlog / cJSON: use OPENEMBER_THIRD_PARTY_MODE (FetchContent / Vendor / System)
openember_third_party_resolve_zlog()
if(OPENEMBER_LOG_BACKEND STREQUAL "ZLOG")
    message(STATUS "├── zlog: ${ZLOG_LIBRARIES}")
    message(STATUS "└── zlog include: ${ZLOG_INCLUDE_DIRS}")
else()
    message(STATUS "├── zlog: (skipped, OPENEMBER_LOG_BACKEND=${OPENEMBER_LOG_BACKEND})")
endif()

if(OPENEMBER_JSON_LIBRARY STREQUAL "CJSON")
    openember_third_party_resolve_cjson()
    set(OPENEMBER_JSON_INCLUDE_DIRS ${CJSON_INCLUDE_DIRS})
    set(OPENEMBER_JSON_LIBRARIES ${CJSON_LIBRARIES})
    message(STATUS "├── JSON: cJSON (${CJSON_LIBRARIES})")
    message(STATUS "└── JSON include: ${CJSON_INCLUDE_DIRS}")
elseif(OPENEMBER_JSON_LIBRARY STREQUAL "NLOHMANN_JSON")
    openember_third_party_resolve_nlohmann_json()
    set(OPENEMBER_JSON_INCLUDE_DIRS "")
    set(OPENEMBER_JSON_LIBRARIES nlohmann_json::nlohmann_json)
    message(STATUS "├── JSON: nlohmann_json (target nlohmann_json::nlohmann_json)")
endif()

# EasyLogger 已删除：仅保留 CMake 选项占位，强制关闭。
set(EASYLOGGER_INCLUDE_DIRS "")
set(EASYLOGGER_LIBRARIES "")
if(BUILD_EASYLOGGER)
    message(WARNING "EasyLogger 已被删除（third_party/easylogger 不再存在），将保持禁用。")
endif()

if(OPENEMBER_COMPONENT_MQTT)
    openember_third_party_resolve_paho_mqtt_c()
    message(STATUS "├── paho: ${PAHO_MQTT_C_LIBRARIES}")
    message(STATUS "└── paho include: ${PAHO_MQTT_C_INCLUDE_DIRS}")
else()
    set(PAHO_MQTT_C_INCLUDE_DIRS "")
    set(PAHO_MQTT_C_LIBRARIES "")
    message(STATUS "├── paho: (skipped, OPENEMBER_COMPONENT_MQTT=OFF)")
endif()

openember_third_party_resolve_sqlite()
message(STATUS "├── sqlite: ${SQLITE_LIBRARIES}")
message(STATUS "└── sqlite include: ${SQLITE_INCLUDE_DIRS}")

openember_third_party_resolve_optional_cxx_deps()
if(OPENEMBER_WITH_YAMLCPP)
    message(STATUS "├── optional: yaml-cpp enabled (target yaml-cpp::yaml-cpp)")
endif()
if(OPENEMBER_WITH_ASIO)
    message(STATUS "├── optional: Asio enabled (target openember::asio)")
endif()
if(OPENEMBER_LOG_BACKEND STREQUAL "SPDLOG")
    message(STATUS "├── log backend: spdlog (target spdlog::spdlog)")
endif()

# Mongoose (embedded web server) is used by apps/services/web_dashboard.
openember_third_party_resolve_mongoose()
message(STATUS "├── mongoose: ${MONGOOSE_LIBRARIES}")
message(STATUS "└── mongoose include: ${MONGOOSE_INCLUDE_DIRS}")

if(OPENEMBER_MSGBUS_USE_LCM)
    openember_transport_resolve_lcm()
    message(STATUS "├── lcm: ${OPENEMBER_LCM_LIBRARIES}")
    message(STATUS "└── lcm include: ${OPENEMBER_LCM_INCLUDE_DIRS}")
elseif(OPENEMBER_MSGBUS_USE_NNG)
    openember_transport_resolve_nng()
    message(STATUS "├── nng: ${OPENEMBER_NNG_LIBRARIES}")
    message(STATUS "└── nng include: ${OPENEMBER_NNG_INCLUDE_DIRS}")
endif()

set(OPENEMBER_LOG_BACKEND_INCLUDES "")
set(OPENEMBER_LOG_BACKEND_LIBS "")
if(OPENEMBER_LOG_BACKEND STREQUAL "ZLOG")
    list(APPEND OPENEMBER_LOG_BACKEND_INCLUDES ${ZLOG_INCLUDE_DIRS})
    list(APPEND OPENEMBER_LOG_BACKEND_LIBS ${ZLOG_LIBRARIES})
elseif(OPENEMBER_LOG_BACKEND STREQUAL "SPDLOG")
    list(APPEND OPENEMBER_LOG_BACKEND_LIBS spdlog::spdlog)
endif()

set( OPENEMBER_EXTERNAL_INCLUDE_DIRS
    ${OPENEMBER_LOG_BACKEND_INCLUDES}
    ${EASYLOGGER_INCLUDE_DIRS}
    ${PAHO_MQTT_C_INCLUDE_DIRS}
    ${OPENEMBER_JSON_INCLUDE_DIRS}
    ${SQLITE_INCLUDE_DIRS}
)

set( OPENEMBER_EXTERNAL_LIBRARIES
    ${OPENEMBER_LOG_BACKEND_LIBS}
    ${EASYLOGGER_LIBRARIES}
    ${PAHO_MQTT_C_LIBRARIES}
    ${OPENEMBER_JSON_LIBRARIES}
    ${SQLITE_LIBRARIES} dl
)

if(OPENEMBER_MSGBUS_USE_NNG)
    add_definitions(-DEMBER_LIBS_USING_NNG)
    list(APPEND OPENEMBER_EXTERNAL_INCLUDE_DIRS ${OPENEMBER_NNG_INCLUDE_DIRS})
    list(APPEND OPENEMBER_EXTERNAL_LIBRARIES ${OPENEMBER_NNG_LIBRARIES})
elseif(OPENEMBER_MSGBUS_USE_LCM)
    add_definitions(-DEMBER_LIBS_USING_LCM)
    list(APPEND OPENEMBER_EXTERNAL_INCLUDE_DIRS ${OPENEMBER_LCM_INCLUDE_DIRS})
    list(APPEND OPENEMBER_EXTERNAL_LIBRARIES ${OPENEMBER_LCM_LIBRARIES})
endif()

set( OPENEMBER_INTERNAL_INCLUDE_DIRS 
    ${CMAKE_BINARY_DIR}/include
    ${CMAKE_SOURCE_DIR}/core/inc
    ${CMAKE_SOURCE_DIR}/components/GSDL/src
    ${CMAKE_SOURCE_DIR}/components/Log
    ${CMAKE_SOURCE_DIR}/components/ppool
    ${CMAKE_SOURCE_DIR}/components/msgbus
    ${CMAKE_SOURCE_DIR}/components/Common
    ${CMAKE_SOURCE_DIR}/modules/Algorithm
    ${CMAKE_SOURCE_DIR}/components/libcsv
    ${CMAKE_SOURCE_DIR}/components/libyaml
)
if(OPENEMBER_COMPONENT_MQTT)
    list(APPEND OPENEMBER_INTERNAL_INCLUDE_DIRS ${CMAKE_SOURCE_DIR}/components/mqtt)
endif()

set( OPENEMBER_INTERNAL_LIBRARIES 
    gdsl
    Log
    ppool
    msgbus
    Common
    Algorithm
    csv
    yaml
)
if(OPENEMBER_COMPONENT_MQTT)
    list(APPEND OPENEMBER_INTERNAL_LIBRARIES openember_mqtt)
endif()

if(NOT DEFINED OPENEMBER_COMPONENT_NETWORK OR OPENEMBER_COMPONENT_NETWORK)
    list(APPEND OPENEMBER_INTERNAL_INCLUDE_DIRS ${CMAKE_SOURCE_DIR}/components/Network)
    list(APPEND OPENEMBER_INTERNAL_LIBRARIES Network)
endif()

add_subdirectory(platform)
if(OPENEMBER_ENABLE_OSAL)
    list(APPEND OPENEMBER_INTERNAL_INCLUDE_DIRS ${CMAKE_SOURCE_DIR}/platform/osal/include)
    list(APPEND OPENEMBER_INTERNAL_LIBRARIES openember_osal)
endif()
if(OPENEMBER_ENABLE_HAL)
    list(APPEND OPENEMBER_INTERNAL_INCLUDE_DIRS ${CMAKE_SOURCE_DIR}/platform/hal/include)
    list(APPEND OPENEMBER_INTERNAL_LIBRARIES openember_hal)
endif()

set( OPENEMBER_INCLUDE_DIRS 
    ${OPENEMBER_EXTERNAL_INCLUDE_DIRS}
    ${OPENEMBER_INTERNAL_INCLUDE_DIRS}
)

set( OPENEMBER_LIBRARIES
    ${OPENEMBER_EXTERNAL_LIBRARIES}
    ${OPENEMBER_INTERNAL_LIBRARIES}
)

add_subdirectory(components)
add_subdirectory(modules)
add_subdirectory(apps)
add_subdirectory(core)

#========================================================
# for unittest, call "cmake .. -DUNIT_TEST=ON"
if(TESTS_ENABLED)
    enable_testing()
    add_subdirectory(test)
endif()
#========================================================
