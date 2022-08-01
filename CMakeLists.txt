cmake_minimum_required(VERSION 3.1.0)

project(Agloo VERSION 1.0.0 LANGUAGES CXX C)

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
include(${CMAKE_SOURCE_DIR}/cmake/AglooCMakeHelper.cmake NO_POLICY_SCOPE)

################################################################################
# Options
################################################################################
option( DEBUG_ENABLED "Whether to enable debug mode" ON)
option( OPTIMIZATION_DISABLED "Whether to disable the optimization of compilation. " OFF)
option( CROSSCOMPILE_ENABLED "Whether to build for arm" OFF)
option( TESTS_ENABLED "Whether to unit test" ON)
option( BUILD_CJSON "Whether compile cjson" OFF)
option( BUILD_MQTT "Whether compile mqtt" OFF)
option( BUILD_MD5 "Whether compile md5" OFF)

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

################################################################################
# Find packages
################################################################################
find_package(ZLOG QUIET)

if(NOT ZLOG_FOUND) 
    message(STATUS "zlog not found! Build from source")
    set(BUILD_ZLOG TRUE)
    set(ZLOG_INCLUDE_DIRS ${CMAKE_SOURCE_DIR}/third_party/zlog/src)
    set(ZLOG_LIBRARIES zlog)
endif()
message(STATUS "├── ${ZLOG_LIBRARIES}")
message(STATUS "└── ${ZLOG_INCLUDE_DIRS}")

find_package(cJSON QUIET)

if(NOT cJSON_FOUND) 
    message(STATUS "cJSON not found! Build from source")
    set(BUILD_CJSON TRUE)
    set(CJSON_INCLUDE_DIRS ${CMAKE_SOURCE_DIR}/third_party/cJSON)
    set(CJSON_LIBRARIES cjson)
endif()
message(STATUS "├── ${CJSON_LIBRARIES}")
message(STATUS "└── ${CJSON_INCLUDE_DIRS}")

find_package(PahoMqttC QUIET)

if(NOT PahoMqttC_FOUND) 
    message(STATUS "PahoMqttC not found! Build from source")
    set(BUILD_MQTT TRUE)
    set(PAHO_MQTT_C_INCLUDE_DIRS ${CMAKE_SOURCE_DIR}/third_party/paho.mqtt.c/src)
    set(PAHO_MQTT_C_LIBRARIES paho-mqtt3a paho-mqtt3c)
endif()
message(STATUS "├── ${PAHO_MQTT_C_LIBRARIES}")
message(STATUS "└── ${PAHO_MQTT_C_INCLUDE_DIRS}")

add_subdirectory(third_party)

set( AGLOO_EXTERNAL_INCLUDE_DIRS
    ${ZLOG_INCLUDE_DIRS}
    ${PAHO_MQTT_C_INCLUDE_DIRS}
    ${CJSON_INCLUDE_DIRS}
)

set( AGLOO_EXTERNAL_LIBRARIES
    ${ZLOG_LIBRARIES}
    ${PAHO_MQTT_C_LIBRARIES}
    ${CJSON_LIBRARIES}
)

set( AGLOO_INTERNAL_INCLUDE_DIRS 
    ${CMAKE_SOURCE_DIR}/inc
    ${CMAKE_SOURCE_DIR}/libs/HAL
    ${CMAKE_SOURCE_DIR}/libs/Log
    ${CMAKE_SOURCE_DIR}/libs/MessageAPI
    ${CMAKE_SOURCE_DIR}/libs/Common
)

set( AGLOO_INCLUDE_DIRS 
    ${AGLOO_EXTERNAL_INCLUDE_DIRS}
    ${AGLOO_INTERNAL_INCLUDE_DIRS}
)

add_subdirectory(libs)

set( AGLOO_INTERNAL_LIBRARIES 
    HAL
    Log
    MessageAPI
    Common
)

set( AGLOO_LIBRARIES
    ${AGLOO_EXTERNAL_LIBRARIES}
    ${AGLOO_INTERNAL_LIBRARIES}
)

add_subdirectory(modules)
add_subdirectory(src)

#========================================================
# for unittest, call "cmake .. -DUNIT_TEST=ON"
if(TESTS_ENABLED)
    enable_testing()
    add_subdirectory(test)
endif()
#========================================================
