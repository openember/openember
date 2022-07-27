# find the cJSON library
message(STATUS "Find the cJSON library")

find_library(CJSON_LIBRARY NAMES cjson)
find_path(CJSON_INCLUDE_DIR NAMES cjson/cJSON.h)

set(CJSON_INCLUDE_DIRS "${CJSON_INCLUDE_DIR}/cjson")
set(CJSON_LIBRARIES ${CJSON_LIBRARY})

if(NOT TARGET cJSON::cJSON)
    if(TARGET "${CJSON_LIBRARY}")
        # Alias if we found the config file
        add_library(cJSON::cJSON ALIAS cjson)
    else()
        add_library(cJSON::cJSON UNKNOWN IMPORTED)
        set_target_properties(cJSON::cJSON PROPERTIES 
            INTERFACE_INCLUDE_DIRECTORIES "${CJSON_INCLUDE_DIRS}" 
            IMPORTED_LINK_INTERFACE_LANGUAGES "C" 
            IMPORTED_LOCATION "${CJSON_LIBRARIES}")
    endif()
endif()

include(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(cJSON
    REQUIRED_VARS CJSON_LIBRARIES CJSON_INCLUDE_DIRS)
