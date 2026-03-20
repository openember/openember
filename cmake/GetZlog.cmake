# zlog 依赖获取脚本（FetchContent），参考 AimRT 的 GetXxx.cmake 组织方式

include(FetchContent)

function(openember_get_zlog)
    # Targets/names used by OpenEmber:
    # - library target: `zlog`
    # - alias target:   `ZLOG::ZLOG`

    set(OPENEMBER_ZLOG_LOCAL_SOURCE "" CACHE PATH "Optional local path override for zlog")

    # Keep the fetched build minimal: disable zlog unit tests by default.
    # zlog uses a plain variable named `UNIT_TEST` (not an option), so force it via cache.
    set(UNIT_TEST OFF CACHE BOOL "Build zlog unit tests" FORCE)

    # 先尝试用 FetchContent 下载；
    # 如果下载出来的源码不包含 CMake（某些 upstream tarball 只有 makefile/README），则回退到仓库内的 third_party/zlog。
    if(OPENEMBER_ZLOG_LOCAL_SOURCE)
        FetchContent_Declare(
            openember_zlog
            SOURCE_DIR ${OPENEMBER_ZLOG_LOCAL_SOURCE}
            OVERRIDE_FIND_PACKAGE
        )
        FetchContent_MakeAvailable(openember_zlog)
    else()
        FetchContent_Declare(
            openember_zlog
            URL ${OPENEMBER_ZLOG_URL}
            DOWNLOAD_EXTRACT_TIMESTAMP TRUE
        )
        FetchContent_Populate(openember_zlog)

        if(EXISTS "${openember_zlog_SOURCE_DIR}/CMakeLists.txt")
            FetchContent_MakeAvailable(openember_zlog)
        else()
            message(WARNING "zlog fetched source has no CMakeLists.txt, fallback to bundled third_party/zlog for CMake build")
            add_subdirectory("${CMAKE_SOURCE_DIR}/third_party/zlog" "${CMAKE_BINARY_DIR}/third_party/zlog" EXCLUDE_FROM_ALL)
        endif()
    endif()

    if(TARGET zlog AND NOT TARGET ZLOG::ZLOG)
        add_library(ZLOG::ZLOG ALIAS zlog)
    endif()

    # Keep legacy variable interface for the rest of OpenEmber.
    set(_zlog_inc "${openember_zlog_SOURCE_DIR}/src")
    if(NOT EXISTS "${_zlog_inc}/zlog.h")
        set(_zlog_inc "${CMAKE_SOURCE_DIR}/third_party/zlog/src")
    endif()
    set(ZLOG_INCLUDE_DIRS ${_zlog_inc} PARENT_SCOPE)
    if(TARGET zlog)
        set(ZLOG_LIBRARIES zlog PARENT_SCOPE)
    else()
        # Fallback: let consumers use plain name if target isn't created for some reason.
        set(ZLOG_LIBRARIES zlog PARENT_SCOPE)
    endif()
endfunction()

