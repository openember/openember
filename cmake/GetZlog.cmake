# zlog：third_party/ 缓存 + build/_deps/zlog-<ver>/，由 cmake/vendor/zlog 包装编译 src/

include(${CMAKE_SOURCE_DIR}/cmake/ThirdPartyArchive.cmake)

function(openember_get_zlog)
    set(UNIT_TEST OFF CACHE BOOL "Build zlog unit tests" FORCE)

    if(OPENEMBER_ZLOG_LOCAL_SOURCE)
        set(_src "${OPENEMBER_ZLOG_LOCAL_SOURCE}")
    else()
        openember_third_party_prepare_stage(_src "${OPENEMBER_ZLOG_CACHE_KEY}" "${OPENEMBER_ZLOG_STAGE_DIR_NAME}"
            "${OPENEMBER_ZLOG_URL}" "src/zlog.h" "")
    endif()

    set(OPENEMBER_ZLOG_FETCHED_SRC "${_src}/src" CACHE INTERNAL "")
    if(NOT EXISTS "${OPENEMBER_ZLOG_FETCHED_SRC}/zlog.h")
        message(FATAL_ERROR "zlog: zlog.h not found under ${OPENEMBER_ZLOG_FETCHED_SRC}")
    endif()

    add_subdirectory(
        "${CMAKE_SOURCE_DIR}/cmake/vendor/zlog"
        "${CMAKE_BINARY_DIR}/_deps/${OPENEMBER_ZLOG_STAGE_DIR_NAME}-build")

    if(TARGET zlog AND NOT TARGET ZLOG::ZLOG)
        add_library(ZLOG::ZLOG ALIAS zlog)
    endif()

    set(ZLOG_INCLUDE_DIRS "${OPENEMBER_ZLOG_FETCHED_SRC}" PARENT_SCOPE)
    set(ZLOG_LIBRARIES zlog PARENT_SCOPE)
endfunction()
