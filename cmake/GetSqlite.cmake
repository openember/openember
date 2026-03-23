# SQLite amalgamation（FetchContent），与仓库内 third_party/sqlite 构建行为对齐：
# - 目标：sqlite3（共享库）、sqlite3-static（静态库，OUTPUT_NAME sqlite3）
# - 头文件目录：amalgamation 解压目录（含 sqlite3.h）

include(FetchContent)

function(openember_get_sqlite)
    if(NOT DEFINED OPENEMBER_SQLITE_LOCAL_SOURCE)
        set(OPENEMBER_SQLITE_LOCAL_SOURCE "" CACHE PATH "Optional local path override for SQLite amalgamation (directory containing sqlite3.c)")
    endif()

    if(OPENEMBER_SQLITE_LOCAL_SOURCE)
        FetchContent_Declare(
            openember_sqlite_amalgamation
            SOURCE_DIR "${OPENEMBER_SQLITE_LOCAL_SOURCE}"
        )
    else()
        FetchContent_Declare(
            openember_sqlite_amalgamation
            URL ${OPENEMBER_SQLITE_URL}
            DOWNLOAD_EXTRACT_TIMESTAMP TRUE
        )
    endif()

    FetchContent_Populate(openember_sqlite_amalgamation)

    set(_root "${openember_sqlite_amalgamation_SOURCE_DIR}")
    if(NOT EXISTS "${_root}/sqlite3.c")
        # 官方 zip 通常包含一层 sqlite-amalgamation-xxxxxx/
        file(GLOB _cand LIST_DIRECTORIES true "${_root}/*")
        foreach(_d IN LISTS _cand)
            if(IS_DIRECTORY "${_d}" AND EXISTS "${_d}/sqlite3.c")
                set(_root "${_d}")
                break()
            endif()
        endforeach()
    endif()

    if(NOT EXISTS "${_root}/sqlite3.c")
        message(FATAL_ERROR "sqlite3.c not found under ${openember_sqlite_amalgamation_SOURCE_DIR}")
    endif()

    if(NOT TARGET sqlite3)
        add_library(sqlite3 SHARED "${_root}/sqlite3.c")
        target_include_directories(sqlite3 PUBLIC "${_root}")
    endif()

    if(NOT TARGET sqlite3-static)
        add_library(sqlite3-static STATIC "${_root}/sqlite3.c")
        set_target_properties(sqlite3-static PROPERTIES OUTPUT_NAME sqlite3)
        target_include_directories(sqlite3-static PUBLIC "${_root}")
    endif()

    set(SQLITE_INCLUDE_DIRS "${_root}" PARENT_SCOPE)
    set(SQLITE_LIBRARIES sqlite3 PARENT_SCOPE)
endfunction()
