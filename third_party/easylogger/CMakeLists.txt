project(easylogger LANGUAGES C)

set( EASYLOGGER_SRCS 
    src/elog.c
    src/elog_async.c
    src/elog_buf.c 
    src/elog_utils.c 
    port/elog_port.c
    plugins/file/elog_file_port.c 
    plugins/file/elog_file.c 
    #plugins/flash/elog_flash_port.c 
    #plugins/flash/elog_flash.c 
)

include_directories(inc)
include_directories(plugins/file)
include_directories(plugins/flash)

add_library(${PROJECT_NAME} SHARED ${EASYLOGGER_SRCS})