# lcm 依赖获取脚本（FetchContent）
#
# 提供：
# - openember_get_lcm()
# - 统一变量接口：
#   - OPENEMBER_LCM_INCLUDE_DIRS
#   - OPENEMBER_LCM_LIBRARIES
#
include(FetchContent)

function(openember_get_lcm)
    if(NOT DEFINED OPENEMBER_LCM_LOCAL_SOURCE)
        set(OPENEMBER_LCM_LOCAL_SOURCE "" CACHE PATH "Optional local path override for lcm")
    endif()

    FetchContent_GetProperties(openember_lcm)
    if(NOT openember_lcm_POPULATED)
        if(OPENEMBER_LCM_LOCAL_SOURCE)
            FetchContent_Declare(
                openember_lcm
                SOURCE_DIR ${OPENEMBER_LCM_LOCAL_SOURCE}
                OVERRIDE_FIND_PACKAGE
            )
        else()
            FetchContent_Declare(
                openember_lcm
                URL ${OPENEMBER_LCM_URL}
                DOWNLOAD_EXTRACT_TIMESTAMP TRUE
                OVERRIDE_FIND_PACKAGE
            )
        endif()

        # Keep the build minimal.
        set(LCM_ENABLE_EXAMPLES OFF CACHE BOOL "Build test and example programs" FORCE)
        set(LCM_ENABLE_TESTS OFF CACHE BOOL "Build unit tests" FORCE)
        set(LCM_ENABLE_PYTHON OFF CACHE BOOL "Build Python bindings and utilities" FORCE)
        set(LCM_ENABLE_JAVA OFF CACHE BOOL "Build Java bindings and utilities" FORCE)
        set(LCM_ENABLE_LUA OFF CACHE BOOL "Build Lua bindings and utilities" FORCE)

        FetchContent_MakeAvailable(openember_lcm)
    endif()

    # lcm headers typically live under <src>/lcm/*
    set(_inc "${openember_lcm_SOURCE_DIR}")

    # lcm target name varies; prefer static.
    set(_lib "")
    if(TARGET lcm-static)
        set(_lib lcm-static)
    elseif(TARGET lcm)
        set(_lib lcm)
    endif()

    if(NOT _lib)
        set(_lib lcm-static)
    endif()

    set(OPENEMBER_LCM_INCLUDE_DIRS ${_inc} PARENT_SCOPE)
    set(OPENEMBER_LCM_LIBRARIES ${_lib} PARENT_SCOPE)
endfunction()

