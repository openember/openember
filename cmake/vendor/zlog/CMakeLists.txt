# OpenEmber 维护的 zlog CMake 包装：上游 HardySimpson/zlog 仅提供 makefile，
# 不包含顶层 CMakeLists.txt。实际源码路径由 OPENEMBER_ZLOG_FETCHED_SRC 指向（FetchContent 后的 .../src）。

cmake_minimum_required(VERSION 3.10)

project(openember_zlog_vendor LANGUAGES C)

set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)

#=====================================
# version of zlog（与 OPENEMBER_ZLOG_VERSION 对齐）
#=====================================
set(CPACK_PACKAGE_VERSION_MAJOR "1")
set(CPACK_PACKAGE_VERSION_MINOR "2")
set(CPACK_PACKAGE_VERSION_PATCH "16")
set(CPACK_PACKAGE_VERSION "${CPACK_PACKAGE_VERSION_MAJOR}.${CPACK_PACKAGE_VERSION_MINOR}.${CPACK_PACKAGE_VERSION_PATCH}")
set(zlog_ver ${CPACK_PACKAGE_VERSION})
set(zlog_so_ver ${CPACK_PACKAGE_VERSION_MAJOR})

message(STATUS "zlog (vendor CMake) version: ${CPACK_PACKAGE_VERSION}")
message(STATUS "zlog upstream sources: ${OPENEMBER_ZLOG_FETCHED_SRC}")

add_definitions("-g -Wall -Wstrict-prototypes")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c99 -pedantic")
set(CMAKE_C_FLAGS_DEBUG "-ggdb3 -DDEBUG")
set(CMAKE_C_FLAGS_RELEASE "-O2")

if(WIN32)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DWINVER=0x0500 -D_WIN32_WINNT=0x0500 ")
endif()

cmake_policy(SET CMP0015 NEW)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/lib")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/lib")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin")

link_directories(${CMAKE_LIBRARY_OUTPUT_DIRECTORY})

find_package(Threads REQUIRED)

if(NOT OPENEMBER_ZLOG_FETCHED_SRC)
    message(FATAL_ERROR "OPENEMBER_ZLOG_FETCHED_SRC is not set (expected absolute path to upstream zlog src/)")
endif()

add_subdirectory(src)

# 上游单元测试目录为 makefile 工程，此处不纳入；需要时在上游源码树中单独构建。
