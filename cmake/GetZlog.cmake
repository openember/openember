# zlog 依赖获取脚本（FetchContent），参考 AimRT 的 GetXxx.cmake 组织方式
#
# 上游 HardySimpson/zlog 根目录无 CMakeLists.txt，由 cmake/vendor/zlog 包装编译。

include(FetchContent)

function(openember_get_zlog)
    # Targets/names used by OpenEmber:
    # - library target: `zlog`
    # - alias target:   `ZLOG::ZLOG`

    if(NOT DEFINED OPENEMBER_ZLOG_LOCAL_SOURCE)
        set(OPENEMBER_ZLOG_LOCAL_SOURCE "" CACHE PATH "Optional local path override for zlog")
    endif()

    # 上游使用普通变量 UNIT_TEST；默认关闭单元测试（包装 CMake 未接入上游 test/ makefile）
    set(UNIT_TEST OFF CACHE BOOL "Build zlog unit tests" FORCE)

    if(OPENEMBER_ZLOG_LOCAL_SOURCE)
        FetchContent_Declare(
            openember_zlog
            SOURCE_DIR "${OPENEMBER_ZLOG_LOCAL_SOURCE}"
        )
    else()
        FetchContent_Declare(
            openember_zlog
            GIT_REPOSITORY https://github.com/HardySimpson/zlog.git
            GIT_TAG ${OPENEMBER_ZLOG_GIT_TAG}
        )
    endif()

    FetchContent_Populate(openember_zlog)

    set(OPENEMBER_ZLOG_FETCHED_SRC "${openember_zlog_SOURCE_DIR}/src" CACHE INTERNAL "")
    if(NOT EXISTS "${OPENEMBER_ZLOG_FETCHED_SRC}/zlog.h")
        message(FATAL_ERROR
            "zlog: zlog.h not found under ${OPENEMBER_ZLOG_FETCHED_SRC}\n"
            "  Check OPENEMBER_ZLOG_GIT_TAG / OPENEMBER_ZLOG_LOCAL_SOURCE.")
    endif()

    add_subdirectory(
        "${CMAKE_SOURCE_DIR}/cmake/vendor/zlog"
        "${CMAKE_BINARY_DIR}/_deps/openember_zlog-build"
    )

    if(TARGET zlog AND NOT TARGET ZLOG::ZLOG)
        add_library(ZLOG::ZLOG ALIAS zlog)
    endif()

    set(ZLOG_INCLUDE_DIRS "${OPENEMBER_ZLOG_FETCHED_SRC}" PARENT_SCOPE)
    set(ZLOG_LIBRARIES zlog PARENT_SCOPE)
endfunction()
