# cppzmq 依赖获取脚本（FetchContent）
#
# cppzmq 是头文件为主的 C++ 绑定库（不需要编译目标），因此我们只导出 include dir。
#
include(FetchContent)

function(openember_get_cppzmq)
    if(NOT DEFINED OPENEMBER_CPPZMQ_LOCAL_SOURCE)
        set(OPENEMBER_CPPZMQ_LOCAL_SOURCE "" CACHE PATH "Optional local path override for cppzmq")
    endif()

    FetchContent_GetProperties(openember_cppzmq)
    if(NOT openember_cppzmq_POPULATED)
        if(OPENEMBER_CPPZMQ_LOCAL_SOURCE)
            FetchContent_Declare(
                openember_cppzmq
                SOURCE_DIR ${OPENEMBER_CPPZMQ_LOCAL_SOURCE}
                OVERRIDE_FIND_PACKAGE
            )
        else()
            FetchContent_Declare(
                openember_cppzmq
                URL ${OPENEMBER_CPPZMQ_URL}
                DOWNLOAD_EXTRACT_TIMESTAMP TRUE
                OVERRIDE_FIND_PACKAGE
            )
        endif()

        # MakeAvailable is needed only if upstream generates targets;
        # it is safe even if no compilation happens.
        FetchContent_MakeAvailable(openember_cppzmq)
    endif()

    set(OPENEMBER_CPPZMQ_INCLUDE_DIRS ${openember_cppzmq_SOURCE_DIR} PARENT_SCOPE)
endfunction()

