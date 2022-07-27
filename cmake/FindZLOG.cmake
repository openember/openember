# find the zlog library
message(STATUS "Find the zlog library")

find_library(ZLOG_LIBRARY NAMES zlog)
find_path(ZLOG_INCLUDE_DIR NAMES zlog.h)

set(ZLOG_INCLUDE_DIRS ${ZLOG_INCLUDE_DIR})
set(ZLOG_LIBRARIES ${ZLOG_LIBRARY})

if(NOT TARGET ZLOG::ZLOG)
    if(TARGET "${ZLOG_LIBRARY}")
        # Alias if we found the config file
        add_library(ZLOG::ZLOG ALIAS zlog)
    else()
        add_library(ZLOG::ZLOG UNKNOWN IMPORTED)
        set_target_properties(ZLOG::ZLOG PROPERTIES 
            INTERFACE_INCLUDE_DIRECTORIES "${ZLOG_INCLUDE_DIRS}" 
            IMPORTED_LINK_INTERFACE_LANGUAGES "C" 
            IMPORTED_LOCATION "${ZLOG_LIBRARIES}")
    endif()
endif()

include(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(ZLOG
    REQUIRED_VARS ZLOG_LIBRARIES ZLOG_INCLUDE_DIRS)
