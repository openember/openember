# nng 依赖获取脚本（FetchContent）
#
# 提供：
# - openember_get_nng()
# - 统一变量接口：
#   - OPENEMBER_NNG_INCLUDE_DIRS
#   - OPENEMBER_NNG_LIBRARIES
#
include(FetchContent)

function(openember_get_nng)
    if(NOT DEFINED OPENEMBER_NNG_LOCAL_SOURCE)
        set(OPENEMBER_NNG_LOCAL_SOURCE "" CACHE PATH "Optional local path override for nng")
    endif()

    FetchContent_GetProperties(openember_nng)
    if(NOT openember_nng_POPULATED)
        if(OPENEMBER_NNG_LOCAL_SOURCE)
            FetchContent_Declare(
                openember_nng
                SOURCE_DIR ${OPENEMBER_NNG_LOCAL_SOURCE}
                OVERRIDE_FIND_PACKAGE
            )
        else()
            FetchContent_Declare(
                openember_nng
                URL ${OPENEMBER_NNG_URL}
                DOWNLOAD_EXTRACT_TIMESTAMP TRUE
                OVERRIDE_FIND_PACKAGE
            )
        endif()

        # Keep the build minimal.
        set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build shared libs" FORCE)
        set(NNG_TESTS OFF CACHE BOOL "Build and run tests." FORCE)
        set(NNG_TOOLS OFF CACHE BOOL "Build extra tools." FORCE)
        set(NNG_ENABLE_COVERAGE OFF CACHE BOOL "Enable coverage reporting." FORCE)
        set(NNG_ENABLE_STATS OFF CACHE BOOL "Enable statistics." FORCE)
        set(NNG_ENABLE_HTTP OFF CACHE BOOL "Enable HTTP API." FORCE)
        set(NNG_TRANSPORT_WS OFF CACHE BOOL "Enable WebSocket transport." FORCE)
        set(NNG_TRANSPORT_WSS OFF CACHE BOOL "Enable WSS transport." FORCE)
        set(NNG_ENABLE_NNGCAT OFF CACHE BOOL "Enable building nngcat utility." FORCE)

        FetchContent_MakeAvailable(openember_nng)
    endif()

    set(_inc "${openember_nng_SOURCE_DIR}/include")

    set(_lib "")
    if(TARGET nng)
        set(_lib nng)
    endif()

    if(NOT _lib)
        set(_lib nng)
    endif()

    set(OPENEMBER_NNG_INCLUDE_DIRS ${_inc} PARENT_SCOPE)
    set(OPENEMBER_NNG_LIBRARIES ${_lib} PARENT_SCOPE)
endfunction()

