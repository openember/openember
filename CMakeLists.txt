cmake_minimum_required(VERSION 3.16)

project(OpenEmber VERSION 1.0.0 LANGUAGES CXX C)

# Stable paths for both a standalone OpenEmber build and an
# add_subdirectory(openember) product build.  CMAKE_SOURCE_DIR belongs to the
# consuming product in the latter case and must never be used as the framework
# source root.
set(OPENEMBER_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
set(OPENEMBER_BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}")

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
list(PREPEND CMAKE_MODULE_PATH "${OPENEMBER_SOURCE_DIR}/cmake")

# Include helper macros and commands, and allow the included file to override
# the CMake policies in this file
include(${OPENEMBER_SOURCE_DIR}/cmake/OpenEmberCMakeHelper.cmake NO_POLICY_SCOPE)
include(${OPENEMBER_SOURCE_DIR}/cmake/OpenEmberProduct.cmake)

# Third-party dependency policy (FetchContent / Vendor / System)
include(${OPENEMBER_SOURCE_DIR}/cmake/Dependencies.cmake)

# Kconfig generated cache/config (optional)
if(EXISTS "${CMAKE_BINARY_DIR}/config.cmake")
    include("${CMAKE_BINARY_DIR}/config.cmake")
endif()

include(${OPENEMBER_SOURCE_DIR}/cmake/OpenEmberThirdPartyBundleDefaults.cmake)

# nlohmann/json 为唯一 JSON 实现（旧 config.cmake 可能仍关闭该 bundle）
set(OPENEMBER_THIRD_PARTY_BUNDLE_NLOHMANN_JSON ON CACHE BOOL "Third-party bundle: nlohmann/json" FORCE)

################################################################################
# Options
################################################################################
option( DEBUG_ENABLED "Whether to enable debug mode" ON)
option( OPTIMIZATION_DISABLED "Whether to disable the optimization of compilation. " OFF)
option( CROSSCOMPILE_ENABLED "Whether to build for ARM" OFF)
option( OPENMP_ENABLED "Whether to enable omp feature" OFF)
option( TESTS_ENABLED "Whether to build/register tests (test/); prefer Kconfig test/Kconfig" OFF)
option( OPENEMBER_ENABLE_OSAL "Build platform OSAL (Linux pthread)" ON)
option( OPENEMBER_ENABLE_LPIO "Build platform LPIO (C++ Linux peripherals)" ON)
option( OPENEMBER_ENABLE_LPIO_EXAMPLES "Build platform LPIO examples (serial_echo, i2c_scan, can_logger)" OFF)
option( OPENEMBER_ENABLE_TOOLS "Build OpenEmber utilities (tools/)" ON)
option( OPENEMBER_ENABLE_TOOLS_EMCOM "Build emcom (picocom-like serial console)" ON)
option( OPENEMBER_ENABLE_TOOLS_SBUS_RECEIVER "Build sbus-receiver (SBUS channel monitor)" ON)
option( OPENEMBER_ENABLE_TOOLS_SENSOR_CLI "Build interactive sensor diagnostic CLI" ON)
option( OPENEMBER_ENABLE_TOOLS_GAMEPAD_MONITOR "Build F710 XInput monitor" ON)
option( EXAMPLES_ENABLED "Whether compile examples" ON)
option( OPENEMBER_COMPONENT_MQTT "Build MQTT component (Paho C + openember_mqtt)" ON)
option( OPENEMBER_COMPONENT_HARDWARE "Build component: Hardware common interfaces" ON)
option( OPENEMBER_COMPONENT_SENSOR "Build component: Sensor Framework" ON)
option( OPENEMBER_SENSOR_YESENSE "Build optional YESENSE YIS321 read-only driver (LPIO)" OFF)
if(OPENEMBER_SENSOR_YESENSE AND NOT OPENEMBER_COMPONENT_SENSOR)
    message(FATAL_ERROR "OPENEMBER_SENSOR_YESENSE requires OPENEMBER_COMPONENT_SENSOR")
endif()
option( OPENEMBER_COMPONENT_ACTUATOR "Build component: Actuator Framework" ON)
option( OPENEMBER_COMPONENT_REMOTE_CONTROL "Build component: Remote control input and mapping" ON)
option( OPENEMBER_COMPONENT_SYSTEM_STATE "Build generic whole-device state reducer" OFF)
option( OPENEMBER_COMPONENT_POWER "Build generic power device interfaces and control validation" OFF)
option( OPENEMBER_COMPONENT_ALERTS "Build component: Alert state, events and logging" ON)
option( OPENEMBER_COMPONENT_ALERTS_LINK "Publish alerts and state through Link" OFF)
option( OPENEMBER_COMPONENT_INFERENCE "Build optional ONNX Runtime CPU inference" OFF)
option( OPENEMBER_MQTT_PAHO_TLS "Build Paho with OpenSSL (TLS / ssl://)" ON)
option( OPENEMBER_MQTT_PAHO_ASYNC "Link Paho MQTTAsync in addition to MQTTClient" OFF)
option( OPENEMBER_EXAMPLE_MQTT_EMQX "Build example mqtt_emqx_client (Paho MQTT C → broker)" OFF)
option( BUILD_MQTT "Whether compile MQTT" OFF)
option( BUILD_MD5 "Whether compile md5" OFF)
option( BUILD_EASYLOGGER "Whether compile EasyLogger (obsolete, currently disabled)" OFF)

option( OPENEMBER_WITH_ZENOHCXX "Use zenoh-cpp C++ bindings without Transport component (requires zenoh-c; experimental)" OFF)
option( OPENEMBER_SERVICE_HARDWARE_INTERFACE "Build service: Hardware Interface" ON)
option( OPENEMBER_HARDWARE_ENDPOINT_IMU "Build Hardware Interface IMU endpoint" ON)
option( OPENEMBER_HARDWARE_ENDPOINT_TEMPERATURE "Build Hardware Interface Temperature endpoint" ON)
option( OPENEMBER_HARDWARE_ENDPOINT_GNSS "Build Hardware Interface GNSS endpoint" ON)
option( OPENEMBER_HARDWARE_ENDPOINT_JOINT_CONTROLLER "Build Hardware Interface Joint Controller endpoint" OFF)

if(NOT DEFINED OPENEMBER_COMPONENT_TRANSPORT)
    set(OPENEMBER_COMPONENT_TRANSPORT ON CACHE BOOL "Build component: Transport (Zenoh)" FORCE)
endif()

# Link 依赖 Transport；openember::core 当前承载 Link 的节点通信 API。
if(NOT DEFINED OPENEMBER_ENABLE_LINK)
    set(OPENEMBER_ENABLE_LINK ON CACHE BOOL "Build OpenEmber Link communication layer" FORCE)
endif()
if(OPENEMBER_ENABLE_LINK AND NOT OPENEMBER_COMPONENT_TRANSPORT)
    message(FATAL_ERROR "OPENEMBER_ENABLE_LINK requires OPENEMBER_COMPONENT_TRANSPORT")
endif()

if(OPENEMBER_ENABLE_LINK)
    set(OPENEMBER_ENABLE_CORE ON CACHE BOOL "Build C++ core (Node/Topic/Service)" FORCE)
else()
    set(OPENEMBER_ENABLE_CORE OFF CACHE BOOL "Build C++ core (Node/Topic/Service)" FORCE)
endif()

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
# Log backend (BUILTIN / SPDLOG) + generated ember_log_backend.h
################################################################################
if(NOT DEFINED OPENEMBER_LOG_BACKEND)
    set(OPENEMBER_LOG_BACKEND "SPDLOG" CACHE STRING "OpenEmber logging backend")
endif()
set_property(CACHE OPENEMBER_LOG_BACKEND PROPERTY STRINGS BUILTIN SPDLOG)

option(OPENEMBER_WITH_YAMLCPP "Fetch/use yaml-cpp (optional C++ dependency)" ON)
option(OPENEMBER_COMPONENT_CONFIG "Build YAML runtime configuration component" ${OPENEMBER_WITH_YAMLCPP})
option(OPENEMBER_WITH_ASIO "Fetch/use standalone Asio (optional)" ON)
option(OPENEMBER_WITH_RUCKIG "Fetch/use ruckig trajectory library (optional; C++20)" OFF)
option(OPENEMBER_ENABLE_LINK "Build OpenEmber Link communication layer" ON)
option(OPENEMBER_ENABLE_MSGS "Build openember-msgs C++ protocol bindings" ON)

set(OPENEMBER_LOG_BACKEND_IS_BUILTIN 0)
set(OPENEMBER_LOG_BACKEND_IS_SPDLOG 0)
if(OPENEMBER_LOG_BACKEND STREQUAL "BUILTIN")
    set(OPENEMBER_LOG_BACKEND_IS_BUILTIN 1)
elseif(OPENEMBER_LOG_BACKEND STREQUAL "SPDLOG")
    set(OPENEMBER_LOG_BACKEND_IS_SPDLOG 1)
else()
    message(FATAL_ERROR "OPENEMBER_LOG_BACKEND must be BUILTIN or SPDLOG (got '${OPENEMBER_LOG_BACKEND}')")
endif()

configure_file(
    ${OPENEMBER_SOURCE_DIR}/core/inc/ember_log_backend.h.in
    ${OPENEMBER_BINARY_DIR}/include/ember_log_backend.h
    @ONLY)

################################################################################
# JSON：nlohmann/json（C++）
################################################################################
set(OPENEMBER_JSON_USE_CJSON 0)
set(OPENEMBER_JSON_USE_NLOHMANN_JSON 1)

configure_file(
    ${OPENEMBER_SOURCE_DIR}/core/inc/ember_json_config.h.in
    ${OPENEMBER_BINARY_DIR}/include/ember_json_config.h
    @ONLY)

################################################################################
# Resolve third-party dependencies
################################################################################

openember_third_party_resolve_nlohmann_json()
set(OPENEMBER_JSON_INCLUDE_DIRS "")
set(OPENEMBER_JSON_LIBRARIES nlohmann_json::nlohmann_json)
message(STATUS "├── JSON: nlohmann_json (target nlohmann_json::nlohmann_json)")

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
if(OPENEMBER_WITH_RUCKIG)
    message(STATUS "├── optional: ruckig enabled (target ruckig::ruckig)")
endif()
if(OPENEMBER_LOG_BACKEND STREQUAL "SPDLOG")
    message(STATUS "├── log backend: spdlog (target spdlog::spdlog)")
endif()

if(OPENEMBER_ENABLE_MSGS)
    openember_third_party_resolve_openember_msgs()
    message(STATUS "├── openember-msgs: ${OPENEMBER_MSGS_LIBRARIES}")
else()
    set(OPENEMBER_MSGS_LIBRARIES "")
    message(STATUS "├── openember-msgs: (skipped, OPENEMBER_ENABLE_MSGS=OFF)")
endif()

# Mongoose (embedded web server) is used by services/web_console.
openember_third_party_resolve_mongoose()
message(STATUS "├── mongoose: ${MONGOOSE_LIBRARIES}")
message(STATUS "└── mongoose include: ${MONGOOSE_INCLUDE_DIRS}")

if(NOT DEFINED OPENEMBER_COMPONENT_TRANSPORT OR OPENEMBER_COMPONENT_TRANSPORT)
    openember_transport_resolve_zenoh_cpp()
    message(STATUS "├── zenoh-c: ${OPENEMBER_ZENOHC_LIBRARIES}")
    message(STATUS "└── zenoh-cpp: ${OPENEMBER_ZENOHCXX_LIBRARIES}")
elseif(OPENEMBER_WITH_ZENOHCXX)
    openember_transport_resolve_zenoh_c()
    message(STATUS "├── zenoh-c: ${OPENEMBER_ZENOHC_LIBRARIES}")
endif()

openember_third_party_prefetch_unused_bundles()

set(OPENEMBER_LOG_BACKEND_INCLUDES "")
set(OPENEMBER_LOG_BACKEND_LIBS "")
if(OPENEMBER_LOG_BACKEND STREQUAL "SPDLOG")
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

set( OPENEMBER_INTERNAL_INCLUDE_DIRS
    ${OPENEMBER_BINARY_DIR}/include
    ${OPENEMBER_SOURCE_DIR}/core/inc
    ${OPENEMBER_SOURCE_DIR}/components/logging
    ${OPENEMBER_SOURCE_DIR}/components/logging/include
)
if(OPENEMBER_COMPONENT_MQTT)
    list(APPEND OPENEMBER_INTERNAL_INCLUDE_DIRS ${OPENEMBER_SOURCE_DIR}/components/mqtt)
endif()

set( OPENEMBER_INTERNAL_LIBRARIES
    logging
)
if(NOT DEFINED OPENEMBER_COMPONENT_NETDEV OR OPENEMBER_COMPONENT_NETDEV)
    list(APPEND OPENEMBER_INTERNAL_INCLUDE_DIRS ${OPENEMBER_SOURCE_DIR}/components/netdev/include)
    list(APPEND OPENEMBER_INTERNAL_LIBRARIES openember_netdev)
endif()
if(NOT DEFINED OPENEMBER_COMPONENT_THREAD_POOL OR OPENEMBER_COMPONENT_THREAD_POOL)
    list(APPEND OPENEMBER_INTERNAL_INCLUDE_DIRS ${OPENEMBER_SOURCE_DIR}/components/thread_pool/include)
    list(APPEND OPENEMBER_INTERNAL_LIBRARIES openember_thread_pool)
endif()
if(NOT DEFINED OPENEMBER_COMPONENT_ALGORITHM OR OPENEMBER_COMPONENT_ALGORITHM)
    list(APPEND OPENEMBER_INTERNAL_INCLUDE_DIRS ${OPENEMBER_SOURCE_DIR}/components/algorithm/include)
    list(APPEND OPENEMBER_INTERNAL_LIBRARIES openember_algorithm)
endif()
if(NOT DEFINED OPENEMBER_COMPONENT_HARDWARE OR OPENEMBER_COMPONENT_HARDWARE)
    list(APPEND OPENEMBER_INTERNAL_INCLUDE_DIRS ${OPENEMBER_SOURCE_DIR}/components/hardware/include)
    list(APPEND OPENEMBER_INTERNAL_LIBRARIES openember_hardware)
endif()
if(NOT DEFINED OPENEMBER_COMPONENT_SENSOR OR OPENEMBER_COMPONENT_SENSOR)
    list(APPEND OPENEMBER_INTERNAL_INCLUDE_DIRS ${OPENEMBER_SOURCE_DIR}/components/sensor/include)
    list(APPEND OPENEMBER_INTERNAL_LIBRARIES openember_sensor)
endif()
if(NOT DEFINED OPENEMBER_COMPONENT_ACTUATOR OR OPENEMBER_COMPONENT_ACTUATOR)
    list(APPEND OPENEMBER_INTERNAL_INCLUDE_DIRS ${OPENEMBER_SOURCE_DIR}/components/actuator/include)
    list(APPEND OPENEMBER_INTERNAL_LIBRARIES openember_actuator)
endif()
if(NOT DEFINED OPENEMBER_COMPONENT_TRANSPORT OR OPENEMBER_COMPONENT_TRANSPORT)
    list(APPEND OPENEMBER_INTERNAL_INCLUDE_DIRS ${OPENEMBER_SOURCE_DIR}/components/transport/include)
    list(APPEND OPENEMBER_INTERNAL_LIBRARIES openember_transport)
endif()
if(OPENEMBER_COMPONENT_MQTT)
    list(APPEND OPENEMBER_INTERNAL_LIBRARIES openember_mqtt)
endif()

if(NOT DEFINED OPENEMBER_COMPONENT_NETWORK OR OPENEMBER_COMPONENT_NETWORK)
    list(APPEND OPENEMBER_INTERNAL_INCLUDE_DIRS ${OPENEMBER_SOURCE_DIR}/components/network)
    list(APPEND OPENEMBER_INTERNAL_LIBRARIES network)
endif()

add_subdirectory(platform)

if(OPENEMBER_ENABLE_TOOLS)
    add_subdirectory(tools)
endif()

if(OPENEMBER_ENABLE_OSAL)
    list(APPEND OPENEMBER_INTERNAL_INCLUDE_DIRS ${OPENEMBER_SOURCE_DIR}/platform/osal/include)
    list(APPEND OPENEMBER_INTERNAL_LIBRARIES openember_osal)
endif()
if(OPENEMBER_ENABLE_LPIO)
    list(APPEND OPENEMBER_INTERNAL_INCLUDE_DIRS ${OPENEMBER_SOURCE_DIR}/platform/lpio/include)
    list(APPEND OPENEMBER_INTERNAL_LIBRARIES openember_lpio)
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
add_subdirectory(core)
if(OPENEMBER_COMPONENT_POWER AND TARGET openember::msgs_cpp)
    add_subdirectory(components/power/protocol)
endif()
if(OPENEMBER_COMPONENT_ALERTS_LINK)
    add_subdirectory(components/alerts/link)
endif()
if(OPENEMBER_ENABLE_CORE)
    list(APPEND OPENEMBER_INTERNAL_INCLUDE_DIRS ${OPENEMBER_SOURCE_DIR}/core/include)
    list(APPEND OPENEMBER_INTERNAL_LIBRARIES openember_core)
    list(APPEND OPENEMBER_INCLUDE_DIRS ${OPENEMBER_SOURCE_DIR}/core/include)
    list(APPEND OPENEMBER_LIBRARIES openember_core)
endif()
add_subdirectory(system)
add_subdirectory(services)
# Sensor tools consume assembled component targets, which must exist first.
if(OPENEMBER_ENABLE_TOOLS AND OPENEMBER_ENABLE_TOOLS_SENSOR_CLI AND OPENEMBER_COMPONENT_SENSOR AND OPENEMBER_COMPONENT_CONFIG)
    add_subdirectory(tools/sensor-cli)
endif()
if(EXAMPLES_ENABLED)
    add_subdirectory(examples)
endif()
add_subdirectory(apps)

# Downstream product repos (openember-demo-*, customer apps). Point at the
# product repository root; that root's CMakeLists.txt should add_subdirectory
# its own apps/. See docs/design/external-product-repos.md.
set(OPENEMBER_EXTERNAL_APPS_ROOT "" CACHE PATH
    "External product repo root to build alongside OpenEmber apps")
if(OPENEMBER_EXTERNAL_APPS_ROOT)
    get_filename_component(_oe_ext_apps "${OPENEMBER_EXTERNAL_APPS_ROOT}" ABSOLUTE)
    if(NOT EXISTS "${_oe_ext_apps}/CMakeLists.txt")
        message(FATAL_ERROR
            "OPENEMBER_EXTERNAL_APPS_ROOT=${OPENEMBER_EXTERNAL_APPS_ROOT} "
            "does not contain CMakeLists.txt")
    endif()
    message(STATUS "External product apps: ${_oe_ext_apps}")
    add_subdirectory("${_oe_ext_apps}" "${CMAKE_BINARY_DIR}/external_apps")
endif()

#========================================================
# Tests are opt-in through Kconfig or -DTESTS_ENABLED=ON.
if(TESTS_ENABLED)
    enable_testing()
    add_subdirectory(test)
endif()
#========================================================
