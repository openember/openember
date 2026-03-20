# libzmq 依赖获取脚本（FetchContent）
#
# 提供：
# - openember_get_libzmq()
# - 统一变量接口：
#   - OPENEMBER_LIBZMQ_INCLUDE_DIRS
#   - OPENEMBER_LIBZMQ_LIBRARIES
#
include(FetchContent)

function(openember_get_libzmq)
    # 目标/命名：
    # - 库目标：libzmq / libzmq-static（具体取决于上游配置）
    # - OpenEmber 使用变量方式：OPENEMBER_LIBZMQ_INCLUDE_DIRS / OPENEMBER_LIBZMQ_LIBRARIES

    if(NOT DEFINED OPENEMBER_LIBZMQ_LOCAL_SOURCE)
        set(OPENEMBER_LIBZMQ_LOCAL_SOURCE "" CACHE PATH "Optional local path override for libzmq")
    endif()

    FetchContent_GetProperties(openember_libzmq)
    if(NOT openember_libzmq_POPULATED)
        if(OPENEMBER_LIBZMQ_LOCAL_SOURCE)
            FetchContent_Declare(
                openember_libzmq
                SOURCE_DIR ${OPENEMBER_LIBZMQ_LOCAL_SOURCE}
                OVERRIDE_FIND_PACKAGE
            )
        else()
            FetchContent_Declare(
                openember_libzmq
                URL ${OPENEMBER_LIBZMQ_URL}
                DOWNLOAD_EXTRACT_TIMESTAMP TRUE
                OVERRIDE_FIND_PACKAGE
            )
        endif()

        # Keep the build minimal: disable any upstream tests.
        set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build shared libs" FORCE)
        set(BUILD_TESTING OFF CACHE BOOL "Build tests (CMake conventional)" FORCE)
        set(BUILD_TESTS OFF CACHE BOOL "Build tests (upstream convention)" FORCE)
        # libzmq uses ZMQ_BUILD_TESTS derived from BUILD_TESTS.
        set(ZMQ_BUILD_TESTS OFF CACHE BOOL "Build libzmq tests" FORCE)

        FetchContent_MakeAvailable(openember_libzmq)
    endif()

    # Export include/library variables for legacy consumers.
    set(_inc "${openember_libzmq_SOURCE_DIR}/include")

    # libzmq target name varies by build options.
    set(_lib "")
    if(TARGET libzmq-static)
        set(_lib libzmq-static)
    elseif(TARGET libzmq)
        set(_lib libzmq)
    endif()

    if(NOT _lib)
        # Fallback: keep the plain name. This helps link in some toolchains where the target
        # is not exported but the plain library name works.
        set(_lib libzmq)
    endif()

    set(OPENEMBER_LIBZMQ_INCLUDE_DIRS ${_inc} PARENT_SCOPE)
    set(OPENEMBER_LIBZMQ_LIBRARIES ${_lib} PARENT_SCOPE)
endfunction()

