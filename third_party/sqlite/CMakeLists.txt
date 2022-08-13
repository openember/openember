project(sqlite3 LANGUAGES C)

# User-configurable options
option(BUILD_STATIC_LIB "Build static libraries" ON)

set( SQLITE_SRCS 
    sqlite3.c
)

# Build shared library
add_library(${PROJECT_NAME} SHARED ${SQLITE_SRCS})

# Build static library
if(BUILD_STATIC_LIB)
    set(ORIGINAL_STATIC_LIB_NAME ${PROJECT_NAME}-static)

    add_library(${ORIGINAL_STATIC_LIB_NAME} STATIC ${SQLITE_SRCS})
    set_target_properties(${ORIGINAL_STATIC_LIB_NAME} PROPERTIES OUTPUT_NAME ${PROJECT_NAME})
endif()

install( TARGETS ${PROJECT_NAME}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)