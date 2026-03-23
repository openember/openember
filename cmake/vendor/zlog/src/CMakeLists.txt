# 编译 FetchContent 拉取的上游 zlog 源码（OPENEMBER_ZLOG_FETCHED_SRC），而非本目录下的 .c 文件。

if(NOT OPENEMBER_ZLOG_FETCHED_SRC)
    message(FATAL_ERROR "OPENEMBER_ZLOG_FETCHED_SRC must be set to upstream zlog src directory")
endif()

file(GLOB ZLOG_SRCS "${OPENEMBER_ZLOG_FETCHED_SRC}/*.c")
if(NOT WIN32)
    list(FILTER ZLOG_SRCS EXCLUDE REGEX ".*/zlog_win\\.c$")
endif()
list(FILTER ZLOG_SRCS EXCLUDE REGEX ".*/zlog-chk-conf\\.c$")

add_library(zlog SHARED ${ZLOG_SRCS})
target_include_directories(zlog PUBLIC "${OPENEMBER_ZLOG_FETCHED_SRC}")
target_link_libraries(zlog PUBLIC Threads::Threads)
if(WIN32)
    target_link_libraries(zlog PUBLIC Ws2_32)
endif()
set_target_properties(zlog PROPERTIES VERSION ${zlog_ver} SOVERSION ${zlog_so_ver})

add_library(zlog_s STATIC ${ZLOG_SRCS})
target_include_directories(zlog_s PUBLIC "${OPENEMBER_ZLOG_FETCHED_SRC}")
target_link_libraries(zlog_s PUBLIC Threads::Threads)
if(WIN32)
    target_link_libraries(zlog_s PUBLIC Ws2_32)
endif()
set_target_properties(zlog_s PROPERTIES OUTPUT_NAME zlog)

add_executable(zlog-chk-conf "${OPENEMBER_ZLOG_FETCHED_SRC}/zlog-chk-conf.c")
target_link_libraries(zlog-chk-conf PRIVATE zlog)

install(TARGETS zlog zlog_s zlog-chk-conf
    COMPONENT zlog
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin)

install(FILES "${OPENEMBER_ZLOG_FETCHED_SRC}/zlog.h"
    COMPONENT zlog
    DESTINATION include)
