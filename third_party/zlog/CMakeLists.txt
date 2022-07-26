cmake_minimum_required(VERSION 3.0)

project(zlog LANGUAGES C)

set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)

#=====================================
# version of zlog
#=====================================
SET(CPACK_PACKAGE_VERSION_MAJOR "1")
SET(CPACK_PACKAGE_VERSION_MINOR "2")
SET(CPACK_PACKAGE_VERSION_PATCH "12")
SET(CPACK_RPM_PACKAGE_RELEASE 1)    #release version.
SET(CPACK_PACKAGE_VERSION "${CPACK_PACKAGE_VERSION_MAJOR}.${CPACK_PACKAGE_VERSION_MINOR}.${CPACK_PACKAGE_VERSION_PATCH}")
SET(zlog_ver ${CPACK_PACKAGE_VERSION})
SET(zlog_so_ver ${CPACK_PACKAGE_VERSION_MAJOR})

message(STATUS "Version  : ${CPACK_PACKAGE_VERSION}")
message(STATUS "Platform : ${CMAKE_SYSTEM}")

add_definitions("-g -Wall -Wstrict-prototypes")
set(CMAKE_C_FLAGS "-std=c99 -pedantic")
set(CMAKE_C_FLAGS_DEBUG "-ggdb3 -DDEBUG")
set(CMAKE_C_FLAGS_RELEASE "-O2")

if (WIN32)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DWINVER=0x0500 -D_WIN32_WINNT=0x0500 ")
endif()

#=====================================================
# lib output path.
cmake_policy(SET CMP0015 NEW)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${zlog_BINARY_DIR}/lib")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${zlog_BINARY_DIR}/lib")
# bin output path.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${zlog_BINARY_DIR}/bin")

message(STATUS "Output   : ${zlog_BINARY_DIR}/lib")
message(STATUS "Output   : ${zlog_BINARY_DIR}/bin")
#=====================================================

#=====================================================
# link path.
link_directories(${CMAKE_LIBRARY_OUTPUT_DIRECTORY})
#=====================================================

#=====================================================
# library depend.
find_package(Threads REQUIRED)
if(NOT CMAKE_THREAD_PREFER_PTHREAD) 
    set(CMAKE_THREAD_PREFER_PTHREAD ${CMAKE_THREAD_LIBS_INIT})
    message(STATUS "thread lib : ${CMAKE_THREAD_LIBS_INIT}")
endif()
message(STATUS "thread lib : ${CMAKE_THREAD_PREFER_PTHREAD}")


#set(CMAKE_THREAD_LIBS_INIT "-lpthread")
#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -pthread")
#set(CMAKE_HAVE_THREADS_LIBRARY 1)
#set(CMAKE_USE_WIN32_THREADS_INIT 0)
#set(CMAKE_USE_PTHREADS_INIT 1)
#set(THREADS_PREFER_PTHREAD_FLAG ON)

add_subdirectory(src)
#add_subdirectory(cpack)

#file(GLOB_RECURSE ZLOG_SOURCE_FILES ${CMAKE_CURRENT_SOURCE_DIR}/src/*.c ${CMAKE_CURRENT_SOURCE_DIR}/src/*.h)

#source_group("zlog" FILES ${ZLOG_SOURCE_FILES})

#add_library(${CMAKE_PROJECT_NAME} SHARED ${ZLOG_SOURCE_FILES})

#========================================================
# for unittest, call "cmake .. -DUNIT_TEST=on"
if(UNIT_TEST)
    enable_testing()
    add_subdirectory(test)
endif()
#========================================================
